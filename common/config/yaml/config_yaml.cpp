/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "yaml-cpp/yaml.h"
extern "C" {
#include "common/config/config_load_configmodule.h"
#include "common/config/config_userapi.h"
void *config_allocate_new(configmodule_interface_t *cfg, int sz, bool autoFree);
void config_check_valptr(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int elt_sz, int nb_elt);
int config_common_getdefault(configmodule_interface_t *cfg, paramdef_t *cfgoption, char *prefix);
}
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace config_yaml {
class YamlConfig {
 public:
  YAML::Node config;
  YamlConfig(std::string filename)
  {
    config = YAML::LoadFile(filename);
  }
};

void SetNonDefault(configmodule_interface_t *cfg, const YAML::Node &node, paramdef_t *param)
{
  auto optname = std::string(param->optname);
  switch (param->type) {
    case TYPE_INT:
      *param->iptr = node[optname].as<int>();
      break;
    case TYPE_UINT:
      *param->uptr = node[optname].as<uint>();
      break;
    case TYPE_STRING: {
      std::string setting = node[optname].as<std::string>();
      config_check_valptr(cfg, param, 1, setting.length() + 1);
      sprintf(*param->strptr, "%s", setting.c_str());
      break;
    }
    case TYPE_INT8:
      *param->i8ptr = node[optname].as<int8_t>();
      break;
    case TYPE_UINT8:
      *param->u8ptr = node[optname].as<uint8_t>();
      break;
    case TYPE_INT16:
      *param->i16ptr = node[optname].as<int16_t>();
      break;
    case TYPE_UINT16:
      *param->u16ptr = node[optname].as<uint16_t>();
      break;
    case TYPE_INT64:
      *param->i64ptr = node[optname].as<int64_t>();
      break;
    case TYPE_UINT64:
      *param->u64ptr = node[optname].as<uint64_t>();
      break;
    case TYPE_DOUBLE:
      *param->dblptr = node[optname].as<double>();
      break;
    case TYPE_MASK:
      *param->uptr = node[optname].as<uint>();
      break;
    case TYPE_STRINGLIST: {
      if (node[optname].IsSequence()) {
        config_check_valptr(cfg, param, sizeof(char *), node[optname].size());
        int i = 0;
        for (const auto &elem : node[optname]) {
          sprintf(param->strlistptr[i], "%s", elem.as<std::string>().c_str());
          i++;
        }
        param->numelt = i;
      } else {
        param->numelt = 0;
      }
      break;
    }
    case TYPE_INTARRAY: {
      if (node[optname].IsSequence() && node[optname].size() > 0) {
        param->numelt = node[optname].size();
        param->iptr =
            (int32_t *)config_allocate_new(cfg, param->numelt * sizeof(*param->iptr), !(param->paramflags & PARAMFLAG_NOFREE));

        for (int i = 0; i < param->numelt; i++) {
          param->iptr[i] = node[optname][i].as<int32_t>();
        }
      } else {
        param->numelt = 0;
      }
      break;
    }
    case TYPE_UINTARRAY: {
      if (node[optname].IsSequence() && node[optname].size() > 0) {
        param->numelt = node[optname].size();
        param->uptr =
            (uint32_t *)config_allocate_new(cfg, param->numelt * sizeof(*param->uptr), !(param->paramflags & PARAMFLAG_NOFREE));

        for (int i = 0; i < param->numelt; i++) {
          param->uptr[i] = node[optname][i].as<uint32_t>();
        }
      } else {
        param->numelt = 0;
      }
      break;
    }
    case TYPE_IPV4ADDR: {
      std::string ipv4addr = node[optname].as<std::string>();
      char* ipv4addr_ptr = strdup(ipv4addr.c_str());
      int rst = config_assign_ipv4addr(cfg, param, ipv4addr_ptr);
      free(ipv4addr_ptr);
      if (rst < 0) {
        fprintf(stderr,"[LIBCONFIG] %s not valid for %s \n", ipv4addr.c_str(), optname.c_str());
      }
      break;
    }
    default:
      AssertFatal(false, "Unhandled type %d", param->type);
  }
  param->paramflags |= PARAMFLAG_PARAMSET;
}

void GetParam(configmodule_interface_t *cfg, const YAML::Node &node, paramdef_t *param)
{
  if (node && node[std::string(param->optname)]) {
    if (param->paramflags & PARAMFLAG_DEPRECATED)
      fprintf(stderr, "[CONFIG] option %s is deprecated and will be removed in a future release\n", param->optname);
    SetNonDefault(cfg, node, param);
  } else {
    config_common_getdefault(cfg, param, nullptr);
  }
}

static YamlConfig *config;
static YAML::Node invalid_node;
} // namespace config_yaml

extern "C" int config_yaml_init(configmodule_interface_t *cfg)
{
  char **cfgP = cfg->cfgP;
  cfg->numptrs = 0;
  pthread_mutex_init(&cfg->memBlocks_mutex, NULL);
  memset(cfg->oneBlock, 0, sizeof(cfg->oneBlock));

  try {
    config_yaml::config = new config_yaml::YamlConfig(std::string(cfgP[0]));
  } catch (const YAML::BadFile &e) {
    fprintf(stderr, "[CONFIG] Error loading YAML file '%s': %s\n", cfgP[0], e.what());
    return -1;
  } catch (const YAML::Exception &e) {
    fprintf(stderr, "[CONFIG] Error parsing YAML file '%s': %s\n", cfgP[0], e.what());
    return -1;
  }
  return 0;
}

extern "C" void config_yaml_end(configmodule_interface_t *cfg)
{
  UNUSED(cfg);
  delete config_yaml::config;
}

YAML::Node find_node(const std::string &prefix, YAML::Node node)
{
  // Iterate through each key in the prefix
  std::stringstream ss(prefix);
  std::string key;
  YAML::Node current = node;

  while (std::getline(ss, key, '.')) {
    if (key.at(0) == '[' && key.back() == ']') {
      // The key is an index to a sequence
      if (!current.IsSequence()) {
        throw std::invalid_argument("Expected a sequence at " + key);
      }
      int index = std::stoi(key.substr(1, key.size() - 2));
      current = current[index];
      continue;
    }
    if (!current.IsMap()) {
      throw std::invalid_argument("Expected a map at " + key);
    }
    if (!current[key]) {
      return config_yaml::invalid_node;
    }
    current = current[key];
  }
  return current;
}

extern "C" int config_yaml_get(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix)
{
  std::string p;
  if (prefix != nullptr) {
    p = std::string(prefix);
  }
  YAML::Node node;
  try {
    node = find_node(p, YAML::Clone(config_yaml::config->config));
    if (node == config_yaml::invalid_node) {
      return -1;
    }
  } catch (const std::invalid_argument &e) {
    fprintf(stderr, "[CONFIG] Error finding node '%s': %s\n", p.c_str(), e.what());
    return -1;
  }
  for (auto i = 0; i < numoptions; i++) {
    if (cfgoptions[i].type != TYPE_STRING && cfgoptions[i].type != TYPE_INTARRAY && cfgoptions[i].type != TYPE_UINTARRAY
        && cfgoptions[i].voidptr == nullptr) {
      config_check_valptr(cfg, &cfgoptions[i], sizeof(void *), 1);
    }
  }
  for (int i = 0; i < numoptions; i++) {
    try {
      config_yaml::GetParam(cfg, node, &cfgoptions[i]);
    } catch (const YAML::Exception &e) {
      fprintf(stderr, "[CONFIG] Config error: prefix: %s, setting: %s, error: %s\n", p.c_str(), cfgoptions[i].optname, e.what());
      return -1;
    }
  }
  return numoptions;
}

extern "C" int config_yaml_getlist(configmodule_interface_t *cfg,
                                   paramlist_def_t *ParamList,
                                   paramdef_t *params,
                                   int numparams,
                                   char *prefix)
{
  char path[512];
  if (prefix != nullptr) {
    sprintf(path, "%s.%s", prefix, ParamList->listname);
  } else {
    sprintf(path, "%s", ParamList->listname);
  }
  ParamList->numelt = 0;
  auto node = find_node(path, YAML::Clone(config_yaml::config->config));
  if (node == config_yaml::invalid_node) {
    return -1;
  }

  ParamList->numelt = node.size();
  if (!node.IsSequence()) {
    return -1;
  }

  if (ParamList->numelt > 0 && params != NULL) {
    ParamList->paramarray = static_cast<paramdef_t **>(config_allocate_new(cfg, ParamList->numelt * sizeof(paramdef_t *), true));

    for (int i = 0; i < ParamList->numelt; i++) {
      ParamList->paramarray[i] = static_cast<paramdef *>(config_allocate_new(cfg, numparams * sizeof(paramdef_t), true));
      memcpy(ParamList->paramarray[i], params, sizeof(paramdef_t) * numparams);

      for (int j = 0; j < numparams; j++) {
        ParamList->paramarray[i][j].strptr = NULL;
        if (ParamList->paramarray[i][j].type != TYPE_STRING) {
          config_check_valptr(cfg, &ParamList->paramarray[i][j], sizeof(void *), 1);
        }
      }

      for (int j = 0; j < numparams; j++) {
        try {
          config_yaml::GetParam(cfg, node[i], &ParamList->paramarray[i][j]);
        } catch (const YAML::Exception &e) {
          fprintf(stderr, "[CONFIG] Config error: prefix: %s, setting: %s, error: %s\n", path, ParamList->paramarray[i][j].optname, e.what());
          return -1;
        }
      }
    }
  }

  return ParamList->numelt;
}

extern "C" void config_yaml_write_parsedcfg(configmodule_interface_t *cfg)
{
  (void)cfg;
}

extern "C" int config_yaml_set(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, char *prefix)
{
  (void)cfg;
  (void)cfgoptions;
  (void)numoptions;
  (void)prefix;
  return 0;
}
