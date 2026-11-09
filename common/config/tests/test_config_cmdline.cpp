/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
extern "C" {
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
}

configmodule_interface_t *uniqCfg = nullptr;

void run_config_test(int argc, char **argv, int expected_numelt, int expected_ret)
{
  configmodule_interface_t *cfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);
  ASSERT_NE(cfg, nullptr);
  uniqCfg = cfg;

  paramdef_t params[] = {UINT16PARAM("test", "", 0, NULL, 0)};
  paramlist_def_t list = {"prefix", NULL, 0};
  int ret = config_getlist(cfg, &list, params, sizeofArray(params), NULL);

  EXPECT_EQ(ret, expected_ret);
  EXPECT_EQ(list.numelt, expected_numelt);

  end_configmodule(cfg);
  uniqCfg = nullptr;
}

TEST(cmdline, cmdline_new_array)
{
  char *array[] = {strdup("test_program"), strdup("--prefix.[0].test"), strdup("1")};
  run_config_test(3, array, 1, 1);
  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, cmdline_no_crash_on_missing_bracket)
{
  char *array[] = {strdup("test_program"), strdup("--prefix.test"), strdup("1")};
  run_config_test(3, array, 0, 0);
  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, cmdline_multiple_elements)
{
  char *array[] = {strdup("test_program"), strdup("--prefix.[0].test"), strdup("1"), strdup("--prefix.[1].test"), strdup("2")};
  run_config_test(sizeofArray(array), array, 2, 2);
  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, cmdline_out_of_order_error)
{
  char *array[] = {strdup("test_program"), strdup("--prefix.[1].test"), strdup("1")};
  run_config_test(sizeofArray(array), array, 0, -1);
  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, cmdline_reverse_order_error)
{
  char *array[] = {strdup("test_program"), strdup("--prefix.[1].test"), strdup("1"), strdup("--prefix.[0].test"), strdup("2")};
  run_config_test(sizeofArray(array), array, 0, -1);
  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, load_file_and_cmdline)
{
  const char *conf_path = "test_config.conf";

  char opt_string[256];
  snprintf(opt_string, sizeof(opt_string), "%s", conf_path);

  char *array[] = {strdup("test_program"), strdup("-O"), strdup(opt_string), strdup("--prefix.[1].test"), strdup("100")};

  configmodule_interface_t *cfg = load_configmodule(sizeofArray(array), array, 0);
  ASSERT_NE(cfg, nullptr);
  uniqCfg = cfg;

  paramdef_t params[] = {UINT16PARAM("test", "", 0, NULL, 0)};
  paramlist_def_t list = {"prefix", NULL, 0};
  int ret = config_getlist(cfg, &list, params, sizeofArray(params), NULL);

  EXPECT_EQ(list.numelt, 2);
  EXPECT_EQ(ret, 2);

  ASSERT_NE(list.paramarray[0], nullptr);
  ASSERT_NE(list.paramarray[0][0].u16ptr, nullptr);
  EXPECT_EQ(*(list.paramarray[0][0].u16ptr), 42);

  ASSERT_NE(list.paramarray[1], nullptr);
  ASSERT_NE(list.paramarray[1][0].u16ptr, nullptr);
  EXPECT_EQ(*(list.paramarray[1][0].u16ptr), 100);

  end_configmodule(cfg);
  uniqCfg = nullptr;

  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, load_file_and_overwrite)
{
  const char *conf_path = "test_config.conf";

  char opt_string[256];
  snprintf(opt_string, sizeof(opt_string), "%s", conf_path);

  char *array[] = {strdup("test_program"), strdup("-O"), strdup(opt_string), strdup("--prefix.[0].test"), strdup("100")};

  configmodule_interface_t *cfg = load_configmodule(5, array, 0);
  ASSERT_NE(cfg, nullptr);
  uniqCfg = cfg;

  paramdef_t params[] = {UINT16PARAM("test", "", 0, NULL, 0)};
  paramlist_def_t list = {"prefix", NULL, 0};
  int ret = config_getlist(cfg, &list, params, sizeofArray(params), NULL);

  EXPECT_EQ(list.numelt, 1);
  EXPECT_EQ(ret, 1);

  ASSERT_NE(list.paramarray[0], nullptr);
  ASSERT_NE(list.paramarray[0][0].u16ptr, nullptr);
  EXPECT_EQ(*(list.paramarray[0][0].u16ptr), 100);

  end_configmodule(cfg);
  uniqCfg = nullptr;

  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

TEST(cmdline, load_file_and_out_of_order_error)
{
  const char *conf_path = "test_config.conf";

  char opt_string[256];
  snprintf(opt_string, sizeof(opt_string), "%s", conf_path);

  char *array[] = {strdup("test_program"), strdup("-O"), strdup(opt_string), strdup("--prefix.[2].test"), strdup("100")};

  configmodule_interface_t *cfg = load_configmodule(sizeofArray(array), array, 0);
  ASSERT_NE(cfg, nullptr);
  uniqCfg = cfg;

  paramdef_t params[] = {UINT16PARAM("test", "", 0, NULL, 0)};
  paramlist_def_t list = {"prefix", NULL, 0};
  int ret = config_getlist(cfg, &list, params, sizeofArray(params), NULL);

  EXPECT_EQ(ret, -1);

  end_configmodule(cfg);
  uniqCfg = nullptr;

  for (auto i = 0U; i < sizeofArray(array); i++) {
    free(array[i]);
  }
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
