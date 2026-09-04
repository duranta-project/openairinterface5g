/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "rrc_gNB_measurements.h"
#include "nr_rrc_defs.h"

int allocate_measurement_object_id(void *_ue)
{
  gNB_RRC_UE_t *ue = _ue;
  int ret = uid_linear_allocator_64_bits_new(&ue->measurement_object_ids);
  DevAssert(ret != 64);
  return ret + 1;
}

void free_measurement_object_id(void *_ue, int mo_id)
{
  gNB_RRC_UE_t *ue = _ue;
  uid_linear_allocator_64_bits_free(&ue->measurement_object_ids, mo_id - 1);
}

int allocate_measurement_id(void *_ue)
{
  gNB_RRC_UE_t *ue = _ue;
  int ret = uid_linear_allocator_64_bits_new(&ue->measurement_ids);
  DevAssert(ret != 64);
  return ret + 1;
}

void free_measurement_id(void *_ue, int mo_id)
{
  gNB_RRC_UE_t *ue = _ue;
  uid_linear_allocator_64_bits_free(&ue->measurement_ids, mo_id - 1);
}

int allocate_report_config_id(void *_ue)
{
  gNB_RRC_UE_t *ue = _ue;
  int ret = uid_linear_allocator_64_bits_new(&ue->report_config_ids);
  DevAssert(ret != 64);
  return ret + 1;
}

void free_report_config_id(void *_ue, int mo_id)
{
  gNB_RRC_UE_t *ue = _ue;
  uid_linear_allocator_64_bits_free(&ue->report_config_ids, mo_id - 1);
}

void reset_all_measurement_ids(void *_ue)
{
  gNB_RRC_UE_t *ue = _ue;

  uid_linear_allocator_64_bits_init(&ue->measurement_object_ids);
  uid_linear_allocator_64_bits_init(&ue->measurement_ids);
  uid_linear_allocator_64_bits_init(&ue->report_config_ids);
}
