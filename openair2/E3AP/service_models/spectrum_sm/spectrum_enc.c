/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "spectrum_enc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <libe3/error_codes.h>

#include "common/utils/LOG/log.h"
#include "spectrum_sensing_ring.h" /* SPECTRUM_SENSING_RING_SHM_NAME */
#include "../../e3_agent.h" /* e3_get_encoding() */
#include "Spectrum-SensingIndication.h"
#include "aper_encoder.h"
#ifdef E3_SM_HAVE_PROTOBUF
#include "e3sm_spectrum.pb-c.h"
#endif

#define RAN_FUNCTION_BUFFER_SIZE 512
#define SPECTRUM_RAN_FUNCTION_NAME "spectrum_sm"
#define SPECTRUM_RAN_FUNCTION_DESCRIPTION \
  "Spectrum service model for sensing range indication, PRB block and sensing policy control"
#define SPECTRUM_RAN_FUNCTION_VERSION 1

/* ASN.1 encoding implementation */
static int spectrum_encode_ran_function_data_asn1(uint8_t **encoded_data, size_t *encoded_size)
{
  if (!encoded_data || !encoded_size) {
    return E3_SM_ERROR_INVALID_PARAM;
  }

  *encoded_data = NULL;
  *encoded_size = 0;

  Spectrum_RanFunctionData_t rf_data;
  memset(&rf_data, 0, sizeof(rf_data));

  const size_t name_len = strlen(SPECTRUM_RAN_FUNCTION_NAME);
  const size_t desc_len = strlen(SPECTRUM_RAN_FUNCTION_DESCRIPTION);

  rf_data.name.buf = malloc(name_len);
  rf_data.description.buf = malloc(desc_len);
  if (!rf_data.name.buf || !rf_data.description.buf) {
    free(rf_data.name.buf);
    free(rf_data.description.buf);
    return E3_SM_ERROR_MEMORY;
  }

  memcpy(rf_data.name.buf, SPECTRUM_RAN_FUNCTION_NAME, name_len);
  rf_data.name.size = name_len;
  rf_data.version = SPECTRUM_RAN_FUNCTION_VERSION;
  memcpy(rf_data.description.buf, SPECTRUM_RAN_FUNCTION_DESCRIPTION, desc_len);
  rf_data.description.size = desc_len;

  uint8_t buffer[RAN_FUNCTION_BUFFER_SIZE];
  asn_enc_rval_t enc_ret = aper_encode_to_buffer(&asn_DEF_Spectrum_RanFunctionData, NULL, &rf_data, buffer, sizeof(buffer));

  free(rf_data.name.buf);
  free(rf_data.description.buf);

  if (enc_ret.encoded == -1) {
    return E3_ENCODE_FAILED;
  }

  *encoded_size = (enc_ret.encoded + 7) / 8;
  *encoded_data = malloc(*encoded_size);
  if (!(*encoded_data)) {
    *encoded_size = 0;
    return E3_SM_ERROR_MEMORY;
  }

  memcpy(*encoded_data, buffer, *encoded_size);
  return E3_SUCCESS;
}

/* JSON encoding implementation */
static int spectrum_encode_ran_function_data_json(uint8_t **encoded_data, size_t *encoded_size)
{
  if (!encoded_data || !encoded_size) {
    return E3_SM_ERROR_INVALID_PARAM;
  }

  *encoded_data = NULL;
  *encoded_size = 0;

  json_object *rf_data = json_object_new_object();
  if (!rf_data) {
    return E3_SM_ERROR_MEMORY;
  }

  json_object_object_add(rf_data, "name", json_object_new_string(SPECTRUM_RAN_FUNCTION_NAME));
  json_object_object_add(rf_data, "version", json_object_new_int(SPECTRUM_RAN_FUNCTION_VERSION));
  json_object_object_add(rf_data, "description", json_object_new_string(SPECTRUM_RAN_FUNCTION_DESCRIPTION));

  const char *json_string = json_object_to_json_string(rf_data);
  if (!json_string) {
    json_object_put(rf_data);
    return E3_ENCODE_FAILED;
  }

  *encoded_size = strlen(json_string);
  *encoded_data = malloc(*encoded_size);
  if (!(*encoded_data)) {
    json_object_put(rf_data);
    *encoded_size = 0;
    return E3_SM_ERROR_MEMORY;
  }

  memcpy(*encoded_data, json_string, *encoded_size);
  json_object_put(rf_data);
  return E3_SUCCESS;
}

#ifdef E3_SM_HAVE_PROTOBUF
static int spectrum_encode_ran_function_data_protobuf(uint8_t **encoded_data, size_t *encoded_size)
{
  if (!encoded_data || !encoded_size) {
    return E3_SM_ERROR_INVALID_PARAM;
  }
  *encoded_data = NULL;
  *encoded_size = 0;

  E3sm__Spectrum__V1__SpectrumRanFunctionData rf = E3SM__SPECTRUM__V1__SPECTRUM_RAN_FUNCTION_DATA__INIT;
  rf.has_name = 1;
  rf.name.data = (uint8_t *)SPECTRUM_RAN_FUNCTION_NAME;
  rf.name.len = strlen(SPECTRUM_RAN_FUNCTION_NAME);
  rf.has_version = 1;
  rf.version = SPECTRUM_RAN_FUNCTION_VERSION;
  rf.has_description = 1;
  rf.description.data = (uint8_t *)SPECTRUM_RAN_FUNCTION_DESCRIPTION;
  rf.description.len = strlen(SPECTRUM_RAN_FUNCTION_DESCRIPTION);

  size_t sz = e3sm__spectrum__v1__spectrum_ran_function_data__get_packed_size(&rf);
  uint8_t *buf = malloc(sz);
  if (!buf) {
    return E3_SM_ERROR_MEMORY;
  }
  e3sm__spectrum__v1__spectrum_ran_function_data__pack(&rf, buf);
  *encoded_data = buf;
  *encoded_size = sz;
  return E3_SUCCESS;
}
#endif

int spectrum_encode_ran_function_data(uint8_t **encoded_data, size_t *encoded_size)
{
  if (e3_get_encoding() == E3_ENCODING_ASN1) {
    return spectrum_encode_ran_function_data_asn1(encoded_data, encoded_size);
  }
  if (e3_get_encoding() == E3_ENCODING_PROTOBUF) {
#ifdef E3_SM_HAVE_PROTOBUF
    return spectrum_encode_ran_function_data_protobuf(encoded_data, encoded_size);
#else
    LOG_E(E3AP, "[SPECTRUM] protobuf encoding not compiled in\n");
    return E3_SM_ERROR_INVALID_PARAM;
#endif
  }
  return spectrum_encode_ran_function_data_json(encoded_data, encoded_size);
}

/* Sensing indication (shm-reference form). Single entry point: the wire format
 * is selected at runtime inside, so callers carry no encoding awareness. Allocator-free on
 * both paths (the shm name aliases the constant string). */
int spectrum_encode_indication(const nr_mac_sensing_publish_meta_t *meta,
                               uint32_t write_idx,
                               uint8_t n_ranges,
                               uint8_t *out_buf,
                               size_t out_buf_size)
{
  if (!meta || !out_buf || out_buf_size == 0) {
    return -1;
  }
  if (n_ranges > MAX_SENSING_RANGES) {
    n_ranges = MAX_SENSING_RANGES;
  }

  if (e3_get_encoding() == E3_ENCODING_ASN1) {
    Spectrum_SensingIndication_t pdu;
    memset(&pdu, 0, sizeof(pdu));

    pdu.timestamp = (long)meta->timestamp_ns;
    pdu.sfn = (long)meta->frame;
    pdu.slot = (long)meta->slot;

    /* beam is OPTIONAL -> asn1c generates a `long*` field; point it at a local. */
    long beam_v = (long)meta->beam;
    pdu.beam = &beam_v;

    /* The name aliases the constant string (zero-copy): APER only reads the
     * OCTET STRING during encode, so there is nothing to free. */
    pdu.shmName.buf = (uint8_t *)SPECTRUM_SENSING_RING_SHM_NAME;
    pdu.shmName.size = (int)(sizeof(SPECTRUM_SENSING_RING_SHM_NAME) - 1u);
    pdu.shmWriteIdx = (long)write_idx;
    pdu.nRanges = (long)n_ranges;

    asn_enc_rval_t r = aper_encode_to_buffer(&asn_DEF_Spectrum_SensingIndication, NULL, &pdu, out_buf, out_buf_size);

    if (r.encoded == -1) {
      return -1;
    }
    /* aper_encode_to_buffer returns BITS -- ceiling-divide to bytes. */
    const size_t bytes = (size_t)((r.encoded + 7) / 8);
    if (bytes > out_buf_size) {
      return -1;
    }
    return (int)bytes;
  } else if (e3_get_encoding() == E3_ENCODING_PROTOBUF) {
#ifdef E3_SM_HAVE_PROTOBUF
    E3sm__Spectrum__V1__SpectrumSensingIndication pdu = E3SM__SPECTRUM__V1__SPECTRUM_SENSING_INDICATION__INIT;
    pdu.has_timestamp = 1;
    pdu.timestamp = (int64_t)meta->timestamp_ns;
    pdu.has_sfn = 1;
    pdu.sfn = (uint32_t)meta->frame;
    pdu.has_slot = 1;
    pdu.slot = (uint32_t)meta->slot;
    pdu.has_beam = 1;
    pdu.beam = (uint32_t)meta->beam;
    pdu.has_shm_name = 1;
    pdu.shm_name.data = (uint8_t *)SPECTRUM_SENSING_RING_SHM_NAME;
    pdu.shm_name.len = sizeof(SPECTRUM_SENSING_RING_SHM_NAME) - 1u;
    pdu.has_shm_write_idx = 1;
    pdu.shm_write_idx = write_idx;
    pdu.has_n_ranges = 1;
    pdu.n_ranges = n_ranges;

    size_t sz = e3sm__spectrum__v1__spectrum_sensing_indication__get_packed_size(&pdu);
    if (sz > out_buf_size) {
      return -1;
    }
    e3sm__spectrum__v1__spectrum_sensing_indication__pack(&pdu, out_buf);
    return (int)sz;
#else
    LOG_E(E3AP, "[SPECTRUM] protobuf encoding not compiled in\n");
    return -1;
#endif
  } else {
    int written = snprintf((char *)out_buf,
                           out_buf_size,
                           "{"
                           "\"sensing_shm\":{"
                           "\"shm_name\":\"" SPECTRUM_SENSING_RING_SHM_NAME
                           "\","
                           "\"write_idx\":%u,"
                           "\"n_ranges\":%u"
                           "},"
                           "\"timestamp\":%" PRIu64
                           ","
                           "\"sfn\":%u,"
                           "\"slot\":%u,"
                           "\"beam\":%u"
                           "}",
                           (unsigned)write_idx,
                           (unsigned)n_ranges,
                           meta->timestamp_ns,
                           (unsigned)meta->frame,
                           (unsigned)meta->slot,
                           (unsigned)meta->beam);
    if (written < 0 || (size_t)written >= out_buf_size) {
      return -1;
    }
    return written;
  }
}
