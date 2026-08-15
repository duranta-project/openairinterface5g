/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
extern "C" {
#include "common/config/config_paramdesc.h"
// TODO: Should use minimal_lib for this but for some reason it doesn't link properly
uint64_t get_softmodem_optmask(void)
{
  return 0;
}
configmodule_interface_t *uniqCfg;

#include "common/utils/LOG/log.h"
#include "common/config/config_load_configmodule.h"
#include "common/config/config_userapi.h"
}
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm>

extern "C" {
int config_yaml_init(configmodule_interface_t *cfg);
void config_yaml_end(configmodule_interface_t *cfg);
int config_yaml_get(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix);
int config_yaml_getlist(configmodule_interface_t *cfg, paramlist_def_t *ParamList, paramdef_t *params, int numparams, char *prefix);
void config_yaml_write_parsedcfg(configmodule_interface_t *cfg);
int config_yaml_set(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix);
}

TEST(yaml_config, yaml_basic) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test1.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);
  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}


TEST(yaml_config, yaml_get_existing_values) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test1.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  // Testing paremters present in the test node
  paramdef_t p = {0};
  for (auto i = 1; i <= 3; i++) {
    sprintf(p.optname, "%s%d", "value", i);
    uint16_t value;
    p.type = TYPE_UINT16;
    p.u16ptr = &value;
    p.paramflags = PARAMFLAG_MANDATORY;
    char prefix[] = "test";
    EXPECT_EQ(config_yaml_get(cfg, &p, 1, prefix), 1);
    EXPECT_EQ(value, i);
  }


  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, yaml_get_non_existing_values) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test1.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  // Testing parameters not present in the test node
  paramdef_t p = {0};
  for (auto i = 4; i <= 5; i++) {
    sprintf(p.optname, "%s%d", "value", i);
    uint16_t value;
    p.type = TYPE_UINT16;
    p.u16ptr = &value;
    p.defuintval = i;
    char prefix[] = "test";
    config_yaml_get(cfg, &p, 1, prefix);
    EXPECT_EQ(value, i);
  }

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_high_recusion) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_recursion.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  // Testing paremters present in the test node
  paramdef_t p = {0};
  for (auto i = 1; i <= 3; i++) {
    sprintf(p.optname, "%s%d", "value", i);
    uint16_t value;
    p.type = TYPE_UINT16;
    p.u16ptr = &value;
    char prefix[] = "test.test1.test2.test3.test4";
    EXPECT_EQ(config_yaml_get(cfg, &p, 1, prefix), 1);
    EXPECT_EQ(value, i);
  }

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_list) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_list.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramlist_def_t param_list = {0};
  sprintf(param_list.listname, "%s", "test");
  paramdef_t params[3] = {0};
  uint16_t value[3];
  for (auto i = 0; i < 3; i++) {
    sprintf(params[i].optname, "%s%d", "value", i+1);
    params[i].type = TYPE_UINT16;
    params[i].u16ptr = &value[i];
  }


  config_yaml_getlist(cfg, &param_list, params, 3, nullptr);

  for (auto i = 0; i < 3; i++) {
    for (auto j = 0; j < 3; j++) {
      EXPECT_EQ(*param_list.paramarray[i][j].u16ptr, i * 3 + j + 1);
    }
  }
  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_string_auto_alloc) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_string.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRING;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringvalue");
  param.defstrval = nullptr;

  // Test automatic allocation of strings
  char prefix[] = "test";
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_NE(param.strptr, nullptr);
  EXPECT_EQ(strcmp(*param.strptr, "testvalue"), 0);
  EXPECT_EQ(cfg->numptrs, 2);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_string_no_realloc) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_string.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRING;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringvalue");
  param.defstrval = nullptr;

  // check that if we at least have a non-null pointer to pointer, only one buffer is allocated
  char prefix[] = "test";
  char* non_null_pointer_to_pointer = nullptr;
  param.strptr = &non_null_pointer_to_pointer;
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_EQ(param.strptr, &non_null_pointer_to_pointer);
  EXPECT_EQ(strcmp(*param.strptr, "testvalue"), 0);
  EXPECT_EQ(cfg->numptrs, 1);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_string_pointer_available) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_string.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRING;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringvalue");
  param.defstrval = nullptr;

  // Test that if a strptr is provided, config module doesnt reallocate it
  char prefix[] = "test";
  char *container = (char *)malloc(sizeof(char) * 30);
  param.strptr = &container;
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_EQ(param.strptr, &container);
  EXPECT_EQ(strcmp(*param.strptr, "testvalue"), 0);
  EXPECT_EQ(cfg->numptrs, 0);
  free(container);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_string_default_value) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_string.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRING;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringvalue");
  param.defstrval = nullptr;

  // Test that if defstrval is not null and value is missing in yaml defstrval is set
  char prefix[] = "test";
  char default_value[] = "default";
  param.defstrval = default_value;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringvalue_missing");
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_NE(param.strptr, nullptr);
  EXPECT_EQ(strcmp(*param.strptr, "default"), 0);
  EXPECT_EQ(cfg->numptrs, 2) << " 2 pointers required, 1 for the string, one for the pointer-to string";

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_stringlist) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_string.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRINGLIST;
  param.strptr = nullptr;
  sprintf(param.optname, "%s", "stringlist");
  param.defstrval = nullptr;

  // Test automatic allocation of string lists
  char prefix[] = "test";
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_NE(param.strptr, nullptr);
  for (auto i = 0; i < param.numelt; i++) {
    std::cout << (param.strlistptr)[i] << std::endl;
  }
  // config_check_valptr allocates maximum list size regardless of the number of elements in list
  EXPECT_EQ(cfg->numptrs, MAX_LIST_SIZE + 1);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_list_of_mappings) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_list_of_mappings.yml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_STRINGLIST;
  param.strlistptr = nullptr;
  sprintf(param.optname, "%s", "list");
  param.defstrval = nullptr;

  // Test automatic allocation of string lists
  char prefix1[] = "test.list.[0]";
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix1), 1);
  EXPECT_NE(param.strptr, nullptr);
  EXPECT_STREQ("value1", param.strlistptr[0]);
  EXPECT_STREQ("value2", param.strlistptr[1]);
  EXPECT_STREQ("value3", param.strlistptr[2]);

  char prefix2[] = "test.list.[1]";
  param.strlistptr = nullptr;
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix2), 1);
  EXPECT_NE(param.strptr, nullptr);
  EXPECT_STREQ("value4", param.strlistptr[0]);
  EXPECT_STREQ("value5", param.strlistptr[1]);
  EXPECT_STREQ("value6", param.strlistptr[2]);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_int_array) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_int_array.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t param = {0};
  param.type = TYPE_INTARRAY;
  param.iptr = nullptr;
  sprintf(param.optname, "%s", "array");
  param.defintarrayval = nullptr;

  // Test automatic allocation of int arrays
  char prefix[] = "test";
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_NE(param.iptr, nullptr);
  ASSERT_EQ(param.numelt, 4);
  EXPECT_EQ(1, param.iptr[0]);
  EXPECT_EQ(2, param.iptr[1]);
  EXPECT_EQ(3, param.iptr[2]);
  EXPECT_EQ(4, param.iptr[3]);

  param.uptr = nullptr;
  param.numelt = 0;
  sprintf(param.optname, "%s", "array2");
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_NE(param.uptr, nullptr);
  ASSERT_EQ(param.numelt, 3);
  EXPECT_EQ(1U, param.uptr[0]);
  EXPECT_EQ(2U, param.uptr[1]);
  EXPECT_EQ(3U, param.uptr[2]);

  param.uptr = nullptr;
  param.numelt = 0;
  sprintf(param.optname, "%s", "non-existent-array");
  EXPECT_EQ(config_yaml_get(cfg, &param, 1, prefix), 1);
  EXPECT_EQ(param.uptr, nullptr);
  ASSERT_EQ(param.numelt, 0);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_read_mapping_as_list) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_read_mapping_as_list.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);
  char cfgstring[] = "fhi_72";

  paramlist_def_t pl = {0};
  strncpy(pl.listname, cfgstring, sizeof(pl.listname) - 1);
  config_yaml_getlist(cfg, &pl, NULL, 0, /* prefix */ NULL);
  EXPECT_NE(pl.numelt, 0);
  EXPECT_EQ(pl.numelt, 3);

  paramdef_t params[4] = {0};
  for (auto i = 0U; i < sizeofArray(params); i++) {
    sprintf(params[i].optname, "%s%d", "element_", i+1);
    params[i].type = TYPE_STRING;
    params[i].paramflags = PARAMFLAG_MANDATORY;
  }
  params[sizeofArray(params) - 1].paramflags = 0;
  int ret = config_yaml_get(cfg, params, sizeofArray(params), cfgstring);
  EXPECT_EQ(ret, 4);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, test_read_ipv4) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_ipv4.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  paramdef_t p = {};
  p.type = TYPE_IPV4ADDR;
  strncpy(p.optname, "ipv4_1", sizeof(p.optname) - 1);
  uint32_t addr = 0;
  p.uptr = &addr;
  p.defstrval = nullptr;
  char prefix[] = "test";
  config_yaml_get(cfg, &p, 1, prefix);
  printf("%x\n", addr);

  strncpy(p.optname, "ipv4_2", sizeof(p.optname) - 1);
  config_yaml_get(cfg, &p, 1, prefix);
  printf("%x\n", addr);

  p.defstrval = "10.0.0.1";
  strncpy(p.optname, "ipv4_3", sizeof(p.optname) - 1);
  config_yaml_get(cfg, &p, 1, prefix);
  printf("%x\n", addr);

  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, yaml_read_str_as_int) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("test_read_str_as_int.yaml");
  EXPECT_EQ(config_yaml_init(cfg), 0);

  // Testing paremters present in the test node
  paramdef_t p = {0};
  uint16_t value;
  p.type = TYPE_UINT16;
  p.u16ptr = &value;
  p.paramflags = PARAMFLAG_MANDATORY;
  char prefix[] = "test";
  sprintf(p.optname, "%s", "var");
  EXPECT_EQ(config_yaml_get(cfg, &p, 1, prefix), -1);


  config_yaml_end(cfg);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, yaml_open_non_existing_file) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("non_existing_file.yaml");
  EXPECT_EQ(config_yaml_init(cfg), -1);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

TEST(yaml_config, malformed_file) {
  configmodule_interface_t *cfg = static_cast<configmodule_interface_t*>(calloc(1, sizeof(*cfg)));
  cfg->cfgP[0] = strdup("malformed.yaml");
  EXPECT_EQ(config_yaml_init(cfg), -1);
  free(cfg->cfgP[0]);
  end_configmodule(cfg);
}

int main(int argc, char** argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

