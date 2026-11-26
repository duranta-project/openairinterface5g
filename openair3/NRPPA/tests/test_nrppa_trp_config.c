/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "openair3/NRPPA/nrppa_gNB_config.h"

int main(int argc, char **argv)
{
  DevAssert(argc == 2);
  const char *ids = "TRPIDs = [7, 9];";
  const char *coordinates = "";
  unsigned num_trps = 2;
  bool valid = true;
  bool has_coordinates = false;
  if (!strcmp(argv[1], "coordinates")) {
    coordinates = "TRPxAxis = [0, 10]; TRPyAxis = [20, 30]; TRPzAxis = [40, 50]; Units = [1, 2];";
    has_coordinates = true;
  } else if (!strcmp(argv[1], "origin")) {
    coordinates = "TRPxAxis = [0, 0]; TRPyAxis = [0, 0]; TRPzAxis = [0, 0]; Units = [0, 0];";
    has_coordinates = true;
  } else if (!strcmp(argv[1], "partial")) {
    coordinates = "TRPxAxis = [0, 10];";
    valid = false;
  } else if (!strcmp(argv[1], "mismatched_coordinates")) {
    coordinates = "TRPxAxis = [0]; TRPyAxis = [20, 30]; TRPzAxis = [40, 50]; Units = [1, 2];";
    valid = false;
  } else if (!strcmp(argv[1], "missing_ids")) {
    ids = "";
    valid = false;
  } else if (!strcmp(argv[1], "mismatched_ids")) {
    ids = "TRPIDs = [7];";
    valid = false;
  } else if (!strcmp(argv[1], "excessive_count")) {
    num_trps = 256;
    valid = false;
  } else {
    DevAssert(!strcmp(argv[1], "ids_only"));
  }

  char path[] = "/tmp/oai-trp-config-XXXXXX";
  const int fd = mkstemp(path);
  DevAssert(fd >= 0);
  FILE *file = fdopen(fd, "w");
  DevAssert(file);
  DevAssert(fprintf(file, "positioning_config = ({ NumTRPs = %u; %s %s });\n", num_trps, ids, coordinates) > 0);
  DevAssert(fclose(file) == 0);
  char program[] = "test_nrppa_trp_config";
  char option[] = "-O";
  char *config_argv[] = {program, option, path};
  uniqCfg = load_configmodule(sizeofArray(config_argv), config_argv, 0);
  DevAssert(uniqCfg);
  logInit();
  trp_info_config_t *config = RCconfig_nr_trp_info();
  DevAssert(unlink(path) == 0);
  DevAssert((config != NULL) == valid);
  if (valid) {
    DevAssert(config->num_trp == 2 && config->trps[0].id == 7 && config->trps[1].id == 9);
    DevAssert(config->has_coordinates == has_coordinates);
    if (!strcmp(argv[1], "coordinates")) {
      DevAssert(config->trps[0].x_axis == 0 && config->trps[1].x_axis == 10);
      DevAssert(config->trps[0].y_axis == 20 && config->trps[1].y_axis == 30);
      DevAssert(config->trps[0].z_axis == 40 && config->trps[1].z_axis == 50);
      DevAssert(config->trps[0].unit == 1 && config->trps[1].unit == 2);
    } else if (has_coordinates) {
      DevAssert(config->trps[0].x_axis == 0 && config->trps[0].y_axis == 0 && config->trps[0].z_axis == 0);
    }
  }
  free(config);
  logTerm();
  end_configmodule(uniqCfg);
  return 0;
}
