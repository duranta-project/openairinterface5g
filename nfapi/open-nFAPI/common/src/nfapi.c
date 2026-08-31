/*
 * Copyright (c) 2001-2016, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * Neither the name of the Cisco Systems, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sched.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <execinfo.h>

#include <nfapi_interface.h>
#include <nfapi_nr_interface_scf.h>
#include <nfapi.h>
#include <debug.h>

// What to do when an error happens (e.g., a push or pull fails)
static inline void on_error()
{
  // show the call stack
  int fd = STDERR_FILENO;
  static const char msg[] = "---stack trace---\n";
  __attribute__((unused)) int r = write(fd, msg, sizeof(msg) - 1);
  void *buffer[100];
  int nptrs = backtrace(buffer, sizeof(buffer) / sizeof(buffer[0]));
  backtrace_symbols_fd(buffer, nptrs, fd);

  // abort();
}

// Fundamental routines

uint8_t push8(uint8_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 1) {
    pOut[0] = in;
    (*out) += 1;
    return 1;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pushs8(int8_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 1) {
    pOut[0] = in;
    (*out) += 1;
    return 1;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t push16(uint16_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 2) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    pOut[1] = (in & 0xFF00) >> 8;
    pOut[0] = (in & 0xFF);
#else
    pOut[0] = (in & 0xFF00) >> 8;
    pOut[1] = (in & 0xFF);
#endif
    (*out) += 2;
    return 2;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pushs16(int16_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 2) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    pOut[1] = (in & 0xFF00) >> 8;
    pOut[0] = (in & 0xFF);
#else
    pOut[0] = (in & 0xFF00) >> 8;
    pOut[1] = (in & 0xFF);
#endif
    (*out) += 2;
    return 2;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t push32(uint32_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 4) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    pOut[3] = (in & 0xFF000000) >> 24;
    pOut[2] = (in & 0xFF0000) >> 16;
    pOut[1] = (in & 0xFF00) >> 8;
    pOut[0] = (in & 0xFF);
#else
    pOut[0] = (in & 0xFF000000) >> 24;
    pOut[1] = (in & 0xFF0000) >> 16;
    pOut[2] = (in & 0xFF00) >> 8;
    pOut[3] = (in & 0xFF);
#endif
    (*out) += 4;
    return 4;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pushs32(int32_t in, uint8_t **out, uint8_t *end)
{
  uint8_t *pOut = *out;

  if ((end - pOut) >= 4) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    pOut[3] = (in & 0xFF000000) >> 24;
    pOut[2] = (in & 0xFF0000) >> 16;
    pOut[1] = (in & 0xFF00) >> 8;
    pOut[0] = (in & 0xFF);
#else
    pOut[0] = (in & 0xFF000000) >> 24;
    pOut[1] = (in & 0xFF0000) >> 16;
    pOut[2] = (in & 0xFF00) >> 8;
    pOut[3] = (in & 0xFF);
#endif
    (*out) += 4;
    return 4;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pull8(uint8_t **in, uint8_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 1) {
    *out = *pIn;
    (*in) += 1;
    return 1;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pulls8(uint8_t **in, int8_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 1) {
    *out = *pIn;
    (*in) += 1;
    return 1;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pull16(uint8_t **in, uint16_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 2) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    *out = ((pIn[1]) << 8) | pIn[0];
#else
    *out = ((pIn[0]) << 8) | pIn[1];
#endif
    (*in) += 2;
    return 2;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pulls16(uint8_t **in, int16_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 2) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    *out = ((pIn[1]) << 8) | pIn[0];
#else
    *out = ((pIn[0]) << 8) | pIn[1];
#endif
    (*in) += 2;
    return 2;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pullx32(uint8_t length, uint8_t **in, uint32_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;
  if (length > 4) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s Can't pull more than 4 bytes (%d) into a uint32_t\n", __FUNCTION__, length);
    on_error();
    return 0;
  }
  if ((end - pIn) >= length) {
    for (int i = 0; i < length; i++) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
      *out |= ((uint32_t)pIn[i] << (8 * i));
#else
      *out = ((uint32_t)pIn[i] << (8 * (length - 1 - i)));
#endif
    }
    (*in) += length;
    return length;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pull32(uint8_t **in, uint32_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 4) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    *out = ((uint32_t)pIn[3] << 24) | (pIn[2] << 16) | (pIn[1] << 8) | pIn[0];
#else
    *out = ((uint32_t)pIn[0] << 24) | (pIn[1] << 16) | (pIn[2] << 8) | pIn[3];
#endif
    (*in) += 4;
    return 4;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t pulls32(uint8_t **in, int32_t *out, uint8_t *end)
{
  uint8_t *pIn = *in;

  if ((end - pIn) >= 4) {
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
    *out = (int32_t)(((uint32_t)pIn[3] << 24) | (pIn[2] << 16) | (pIn[1] << 8) | pIn[0]);
#else
    *out = (int32_t)(((uint32_t)pIn[0] << 24) | (pIn[1] << 16) | (pIn[2] << 8) | pIn[3]);
#endif
    (*in) += 4;
    return 4;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

/*
inline void pusharray16(uint8_t **, uint16_t, uint32_t len)
{
}
*/

uint32_t pullarray16(uint8_t **in, uint16_t out[], uint32_t max_len, uint32_t len, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*in)) >= sizeof(uint16_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!pull16(in, &out[idx], end))
        return 0;
    }

    return sizeof(uint16_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint32_t pullarrays16(uint8_t **in, int16_t out[], uint32_t max_len, uint32_t len, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*in)) >= sizeof(uint16_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!pulls16(in, &out[idx], end))
        return 0;
    }

    return sizeof(uint16_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pusharray16(const uint16_t in[], uint32_t max_len, uint32_t len, uint8_t **out, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*out)) >= sizeof(uint16_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!push16(in[idx], out, end))
        return 0;
    }

    return sizeof(uint16_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pusharrays16(int16_t in[], uint32_t max_len, uint32_t len, uint8_t **out, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*out)) >= sizeof(uint16_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!pushs16(in[idx], out, end))
        return 0;
    }

    return sizeof(uint16_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pullarray32(uint8_t **values_to_pull,
                     uint32_t out[],
                     uint32_t max_num_values_to_pull,
                     uint32_t num_values_to_pull,
                     uint8_t *out_end)
{
  if (num_values_to_pull == 0)

    return 1;

  if (num_values_to_pull > max_num_values_to_pull) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, num_values_to_pull, max_num_values_to_pull);
    on_error();
    return 0;
  }

  if ((out_end - (*values_to_pull)) >= sizeof(uint32_t) * num_values_to_pull) {
    uint32_t idx;

    for (idx = 0; idx < num_values_to_pull; ++idx) {
      if (!pull32(values_to_pull, &out[idx], out_end))
        return 0;
    }

    return sizeof(uint32_t) * num_values_to_pull;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint32_t pullarrays32(uint8_t **in, int32_t out[], uint32_t max_len, uint32_t len, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*in)) >= sizeof(uint32_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!pulls32(in, &out[idx], end))
        return 0;
    }

    return sizeof(uint32_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pusharray32(const uint32_t *values_to_push,
                     uint32_t max_num_values_to_push,
                     uint32_t num_values_to_push,
                     uint8_t **out,
                     uint8_t *out_end)
{
  if (num_values_to_push == 0)
    return 1;

  if (num_values_to_push > max_num_values_to_push) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, num_values_to_push, max_num_values_to_push);
    on_error();
    return 0;
  }

  if ((out_end - (*out)) >= sizeof(uint32_t) * num_values_to_push) {
    uint32_t idx;

    for (idx = 0; idx < num_values_to_push; ++idx) {
      if (!push32(values_to_push[idx], out, out_end))
        return 0;
    }

    return sizeof(uint32_t) * num_values_to_push;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pusharrays32(int32_t in[], uint32_t max_len, uint32_t len, uint8_t **out, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*out)) >= sizeof(uint32_t) * len) {
    uint32_t idx;

    for (idx = 0; idx < len; ++idx) {
      if (!pushs32(in[idx], out, end))
        return 0;
    }

    return sizeof(uint32_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}
uint32_t pullarray8(uint8_t **in, uint8_t out[], uint32_t max_len, uint32_t len, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*in)) >= sizeof(uint8_t) * len) {
    memcpy(out, (*in), len);
    (*in) += len;
    return sizeof(uint8_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint32_t pusharray8(uint8_t in[], uint32_t max_len, uint32_t len, uint8_t **out, uint8_t *end)
{
  if (len == 0)
    return 1;

  if (len > max_len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, len, max_len);
    on_error();
    return 0;
  }

  if ((end - (*out)) >= sizeof(uint8_t) * len) {
    memcpy((*out), in, len);
    (*out) += len;
    return sizeof(uint8_t) * len;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s no space in buffer\n", __FUNCTION__);
    on_error();
    return 0;
  }
}

uint8_t packarray(void *array,
                  uint16_t array_element_size,
                  uint16_t max_count,
                  uint16_t count,
                  uint8_t **ppwritepackedmsg,
                  uint8_t *end,
                  pack_array_elem_fn fn)
{
  if (count > max_count) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, count, max_count);
    on_error();
    return 0;
  }

  uint16_t i = 0;

  for (i = 0; i < count; ++i) {
    if ((fn)(array, ppwritepackedmsg, end) == 0)
      return 0;

    array += array_element_size;
  }

  return 1;
}

uint8_t unpackarray(uint8_t **ppReadPackedMsg,
                    void *array,
                    uint16_t array_element_size,
                    uint16_t max_count,
                    uint16_t count,
                    uint8_t *end,
                    unpack_array_elem_fn fn)
{
  if (count > max_count) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s exceed array size (%d > %d)\n", __FUNCTION__, count, max_count);
    on_error();
    return 0;
  }

  uint16_t i = 0;

  for (i = 0; i < count; ++i) {
    if ((fn)(array, ppReadPackedMsg, end) == 0)
      return 0;

    array += array_element_size;
  }

  return 1;
}

uint32_t pack_dci_payload(uint8_t *payload, uint16_t payloadSizeBits, uint8_t **out, uint8_t *end)
{
  uint8_t dci_byte_len = (payloadSizeBits + 7) / 8;
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
  // Helper vars for DCI Payload
  uint8_t dci_bytes_inverted[DCI_PAYLOAD_BYTE_LEN] = {0};
  uint8_t payload_internal[DCI_PAYLOAD_BYTE_LEN] = {0}; // Used to not edit the "outside" pointer
  uint8_t rotation_bits = 0;
  // Align the dci payload bits to the left on the payload buffer
  if (payloadSizeBits % 8 != 0) {
    rotation_bits = 8 - (payloadSizeBits % 8);
    // Bit shifting value ( << )
    uint64_t t = 0;
    memcpy(&t, payload, dci_byte_len);
    t = t << rotation_bits;
    memcpy(payload_internal, &t, dci_byte_len);
  } else {
    // No rotation needed
    memcpy(payload_internal, payload, dci_byte_len);
  }
  // Invert the byte order of the DCI Payload
  for (int j = 0; j < dci_byte_len; j++) {
    dci_bytes_inverted[j] = payload_internal[(dci_byte_len - 1) - j];
  }
  return pusharray8(dci_bytes_inverted, DCI_PAYLOAD_BYTE_LEN, dci_byte_len, out, end);
#else
  return pusharray8(payload, DCI_PAYLOAD_BYTE_LEN, dci_byte_len, out, end);
#endif
}

uint32_t unpack_dci_payload(uint8_t *payload, uint16_t payloadSizeBits, uint8_t **in, uint8_t *end)
{
  // Pull the inverted DCI and invert it back
  //  Helper vars for DCI Payload
  uint8_t dci_byte_len = (payloadSizeBits + 7) / 8;
#ifdef FAPI_BYTE_ORDERING_BIG_ENDIAN
  uint8_t dci_bytes_inverted[DCI_PAYLOAD_BYTE_LEN] = {0};
  // Get DCI array inverted
  uint32_t pullresult = pullarray8(in, dci_bytes_inverted, DCI_PAYLOAD_BYTE_LEN, dci_byte_len, end);

  // Reversing the byte order of the inverted DCI payload
  for (uint16_t j = 0; j < dci_byte_len; j++) {
    payload[j] = dci_bytes_inverted[(dci_byte_len - 1) - j];
  }

  uint64_t t = 0;
  memcpy(&t, payload, dci_byte_len);
  if (payloadSizeBits % 8 != 0) {
    uint8_t rotation_bits = 8 - (payloadSizeBits % 8);
    t = (t >> (uint64_t)rotation_bits);
  }
  memcpy(payload, &t, dci_byte_len);
#else
  uint32_t pullresult = pullarray8(in, payload, DCI_PAYLOAD_BYTE_LEN, dci_byte_len, end);
#endif
  return pullresult;
}

uint32_t pack_vendor_extension_tlv(nfapi_tl_t *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *config)
{
  if (ve != 0 && config != 0) {
    if (config->pack_vendor_extension_tlv) {
      uint8_t *pStartOfTlv = *ppWritePackedMsg;

      if (pack_tl(ve, ppWritePackedMsg, end) == 0)
        return 0;

      uint8_t *pStartOfValue = *ppWritePackedMsg;

      if ((config->pack_vendor_extension_tlv)(ve, ppWritePackedMsg, end, config) == 0)
        return 0;

      ve->length = (*ppWritePackedMsg) - pStartOfValue;
      pack_tl(ve, &pStartOfTlv, end);
      return 1;
    }
  }

  return 1;
}

int unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                uint8_t **ppReadPackedMsg,
                                uint8_t *end,
                                nfapi_p4_p5_codec_config_t *config,
                                nfapi_tl_t **ve_tlv)
{
  if (ve_tlv != 0 && config != 0) {
    if (config->unpack_vendor_extension_tlv) {
      return (config->unpack_vendor_extension_tlv)(tl, ppReadPackedMsg, end, (void **)ve_tlv, config);
    }
  }

  return 1;
}

uint32_t pack_p7_vendor_extension_tlv(nfapi_tl_t *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  if (ve != 0 && config != 0) {
    if (config->pack_vendor_extension_tlv) {
      uint8_t *pStartOfTlv = *ppWritePackedMsg;

      if (pack_tl(ve, ppWritePackedMsg, end) == 0)
        return 0;

      uint8_t *pStartOfValue = *ppWritePackedMsg;

      if ((config->pack_vendor_extension_tlv)(ve, ppWritePackedMsg, end, config) == 0)
        return 0;

      ve->length = (*ppWritePackedMsg) - pStartOfValue;
      pack_tl(ve, &pStartOfTlv, end);
      return 1;
    }
  }

  return 1;
}

int unpack_p7_vendor_extension_tlv(nfapi_tl_t *tl,
                                   uint8_t **ppReadPackedMsg,
                                   uint8_t *end,
                                   nfapi_p7_codec_config_t *config,
                                   nfapi_tl_t **ve_tlv)
{
  if (ve_tlv != 0 && config != 0) {
    if (config->unpack_vendor_extension_tlv) {
      return (config->unpack_vendor_extension_tlv)(tl, ppReadPackedMsg, end, (void **)ve_tlv, config);
    }
  }

  return 1;
}

uint8_t pack_tl(nfapi_tl_t *tl, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  return (push16(tl->tag, ppWritePackedMsg, end) && push16(tl->length, ppWritePackedMsg, end));
}

uint8_t unpack_tl(uint8_t **ppReadPackedMsg, nfapi_tl_t *tl, uint8_t *end)
{
  return (pull16(ppReadPackedMsg, &tl->tag, end) && pull16(ppReadPackedMsg, &tl->length, end));
}

int unpack_tlv_list(unpack_tlv_t unpack_fns[],
                    uint16_t size,
                    uint8_t **ppReadPackedMsg,
                    uint8_t *end,
                    nfapi_p4_p5_codec_config_t *config,
                    nfapi_tl_t **ve)
{
  nfapi_tl_t generic_tl;
  uint8_t numBadTags = 0;
  uint16_t idx = 0;

  while ((uint8_t *)(*ppReadPackedMsg) < end) {
    // unpack the tl and process the values accordingly
    if (unpack_tl(ppReadPackedMsg, &generic_tl, end) == 0)
      return 0;

    uint8_t tagMatch = 0;
    uint8_t *pStartOfValue = *ppReadPackedMsg;

    for (idx = 0; idx < size; ++idx) {
      if (unpack_fns[idx].tag == generic_tl.tag) { // match the extracted tag value with all the tags in unpack_fn list
        tagMatch = 1;
        nfapi_tl_t *tl = (nfapi_tl_t *)(unpack_fns[idx].tlv);
        tl->tag = generic_tl.tag;
        tl->length = generic_tl.length;
        int result = (*unpack_fns[idx].unpack_func)(tl, ppReadPackedMsg, end);

        if (result == 0) {
          return 0;
        }

        // check if the length was right;
        if (tl->length != (*ppReadPackedMsg - pStartOfValue)) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                      tl->tag,
                      tl->length,
                      (*ppReadPackedMsg - pStartOfValue));
          on_error();
        }
      }
    }

    if (tagMatch == 0) {
      if (generic_tl.tag >= NFAPI_VENDOR_EXTENSION_MIN_TAG_VALUE && generic_tl.tag <= NFAPI_VENDOR_EXTENSION_MAX_TAG_VALUE) {
        int result = unpack_vendor_extension_tlv(&generic_tl, ppReadPackedMsg, end, config, ve);

        if (result == 0) {
          // got tot the end.
          return 0;
        } else if (result < 0) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown VE TAG value: 0x%04x\n", generic_tl.tag);
          on_error();

          if (++numBadTags > MAX_BAD_TAG) {
            NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
            on_error();
            return 0;
          }

          if ((end - *ppReadPackedMsg) >= generic_tl.length) {
            // Advance past the unknown TLV
            (*ppReadPackedMsg) += generic_tl.length;
          } else {
            // go to the end
            return 0;
          }
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
        on_error();

        if (++numBadTags > MAX_BAD_TAG) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
          on_error();
          return 0;
        }

        if ((end - *ppReadPackedMsg) >= generic_tl.length) {
          // Advance past the unknown TLV
          (*ppReadPackedMsg) += generic_tl.length;
        } else {
          // go to the end
          return 0;
        }
      }
    }
  }

  return 1;
}

int unpack_nr_tlv_list(unpack_tlv_t unpack_fns[],
                       uint16_t size,
                       uint8_t **ppReadPackedMsg,
                       uint8_t *end,
                       nfapi_p4_p5_codec_config_t *config,
                       nfapi_tl_t **ve)
{
  nfapi_tl_t generic_tl;
  uint8_t numBadTags = 0;
  uint16_t idx = 0;
  if (size == 0) { return 1; }
  while ((uint8_t *)(*ppReadPackedMsg) + 4 < end) {
    // unpack the tl and process the values accordingly
    if (unpack_tl(ppReadPackedMsg, &generic_tl, end) == 0)
      return 0;

    uint8_t tagMatch = 0;
    uint8_t *pStartOfValue = *ppReadPackedMsg;

    for (idx = 0; idx < size; ++idx) {
      if (unpack_fns[idx].tag == generic_tl.tag) { // match the extracted tag value with all the tags in unpack_fn list
        tagMatch = 1;

        if (generic_tl.tag == NFAPI_NR_PARAM_TLV_NUM_CONFIG_TLVS_TO_REPORT_TAG) {
          // padding and sub tlvs handled already
          nfapi_nr_cell_param_t *cellParamTable = (nfapi_nr_cell_param_t *)unpack_fns[idx].tlv;
          cellParamTable->num_config_tlvs_to_report.tl.tag = generic_tl.tag;
          cellParamTable->num_config_tlvs_to_report.tl.length = generic_tl.length;
          int result = (*unpack_fns[idx].unpack_func)(unpack_fns[idx].tlv, ppReadPackedMsg, end);
          if (result == 0) {
            return 0;
          }
          continue;
        }

        nfapi_tl_t *tl = (nfapi_tl_t *)(unpack_fns[idx].tlv);
        tl->tag = generic_tl.tag;
        tl->length = generic_tl.length;
        int result = (*unpack_fns[idx].unpack_func)(tl, ppReadPackedMsg, end);

        if (result == 0) {
          return 0;
        }

        // check if the length was right;
        if (tl->length != (*ppReadPackedMsg - pStartOfValue)) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                      tl->tag,
                      tl->length,
                      (*ppReadPackedMsg - pStartOfValue));
          on_error();
        }
        // Remove padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
        int padding = get_tlv_padding(tl->length);
        if (padding != 0) {
          (*ppReadPackedMsg) += padding;
        }
      }
    }

    if (tagMatch == 0) {
      if (generic_tl.tag >= NFAPI_VENDOR_EXTENSION_MIN_TAG_VALUE && generic_tl.tag <= NFAPI_VENDOR_EXTENSION_MAX_TAG_VALUE) {
        int result = unpack_vendor_extension_tlv(&generic_tl, ppReadPackedMsg, end, config, ve);

        if (result == 0) {
          // got tot the end.
          return 0;
        } else if (result < 0) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown VE TAG value: 0x%04x\n", generic_tl.tag);
          on_error();

          if (++numBadTags > MAX_BAD_TAG) {
            NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
            on_error();
            return 0;
          }

          if ((end - *ppReadPackedMsg) >= generic_tl.length) {
            // Advance past the unknown TLV
            (*ppReadPackedMsg) += generic_tl.length + get_tlv_padding(generic_tl.length);
          } else {
            // go to the end
            return 0;
          }
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
        on_error();

        if (++numBadTags > MAX_BAD_TAG) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
          on_error();
          return 0;
        }

        if ((end - *ppReadPackedMsg) >= generic_tl.length) {
          // Advance past the unknown TLV
          (*ppReadPackedMsg) += generic_tl.length + get_tlv_padding(generic_tl.length);
        } else {
          // go to the end
          return 0;
        }
      }
    }
  }
  return 1;
}

int unpack_p7_tlv_list(unpack_p7_tlv_t unpack_fns[],
                       uint16_t size,
                       uint8_t **ppReadPackedMsg,
                       uint8_t *end,
                       nfapi_p7_codec_config_t *config,
                       nfapi_tl_t **ve)
{
  nfapi_tl_t generic_tl;
  uint8_t numBadTags = 0;
  uint16_t idx = 0;

  while ((uint8_t *)(*ppReadPackedMsg) < end) {
    // unpack the tl and process the values accordingly
    if (unpack_tl(ppReadPackedMsg, &generic_tl, end) == 0)
      return 0;

    uint8_t tagMatch = 0;
    uint8_t *pStartOfValue = *ppReadPackedMsg;

    for (idx = 0; idx < size; ++idx) {
      if (unpack_fns[idx].tag == generic_tl.tag) {
        tagMatch = 1;
        nfapi_tl_t *tl = (nfapi_tl_t *)(unpack_fns[idx].tlv);
        tl->tag = generic_tl.tag;
        tl->length = generic_tl.length;
        int result = (*unpack_fns[idx].unpack_func)(tl, ppReadPackedMsg, end, config);

        if (result == 0) {
          return 0;
        }

        // check if the length was right;
        if (tl->length != (*ppReadPackedMsg - pStartOfValue)) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                      tl->tag,
                      tl->length,
                      (*ppReadPackedMsg - pStartOfValue));
          on_error();
        }
      }
    }

    if (tagMatch == 0) {
      if (generic_tl.tag >= NFAPI_VENDOR_EXTENSION_MIN_TAG_VALUE && generic_tl.tag <= NFAPI_VENDOR_EXTENSION_MAX_TAG_VALUE) {
        int result = unpack_p7_vendor_extension_tlv(&generic_tl, ppReadPackedMsg, end, config, ve);

        if (result == 0) {
          // got to end
          return 0;
        } else if (result < 0) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
          on_error();

          if (++numBadTags > MAX_BAD_TAG) {
            NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
            on_error();
            return -1;
          }

          if ((end - *ppReadPackedMsg) >= generic_tl.length) {
            // Advance past the unknown TLV
            (*ppReadPackedMsg) += generic_tl.length;
          } else {
            // got ot the dn
            return 0;
          }
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
        on_error();

        if (++numBadTags > MAX_BAD_TAG) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
          on_error();
          return -1;
        }

        if ((end - *ppReadPackedMsg) >= generic_tl.length) {
          // Advance past the unknown TLV
          (*ppReadPackedMsg) += generic_tl.length;
        } else {
          // got ot the dn
          return 0;
        }
      }
    }
  }

  return 1;
}

int unpack_nr_p7_tlv_list(unpack_p7_tlv_t unpack_fns[],
                          uint16_t size,
                          uint8_t **ppReadPackedMsg,
                          uint8_t *end,
                          nfapi_p7_codec_config_t *config,
                          nfapi_tl_t **ve)
{
  nfapi_tl_t generic_tl;
  uint8_t numBadTags = 0;
  uint16_t idx = 0;

  while ((uint8_t *)(*ppReadPackedMsg) < end) {
    // unpack the tl and process the values accordingly
    if (unpack_tl(ppReadPackedMsg, &generic_tl, end) == 0)
      return 0;

    uint8_t tagMatch = 0;
    uint8_t *pStartOfValue = *ppReadPackedMsg;

    for (idx = 0; idx < size; ++idx) {
      if (unpack_fns[idx].tag == generic_tl.tag) {
        tagMatch = 1;
        nfapi_tl_t *tl = (nfapi_tl_t *)(unpack_fns[idx].tlv);
        tl->tag = generic_tl.tag;
        tl->length = generic_tl.length;
        int result = (*unpack_fns[idx].unpack_func)(tl, ppReadPackedMsg, end, config);

        if (result == 0) {
          return 0;
        }

        // check if the length was right;
        if (tl->length != (*ppReadPackedMsg - pStartOfValue)) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                      tl->tag,
                      tl->length,
                      (*ppReadPackedMsg - pStartOfValue));
          on_error();
        }
        // Remove padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
        int padding = get_tlv_padding(tl->length);
        if (padding != 0) {
          (*ppReadPackedMsg) += padding;
        }
      }
    }

    if (tagMatch == 0) {
      if (generic_tl.tag >= NFAPI_VENDOR_EXTENSION_MIN_TAG_VALUE && generic_tl.tag <= NFAPI_VENDOR_EXTENSION_MAX_TAG_VALUE) {
        int result = unpack_p7_vendor_extension_tlv(&generic_tl, ppReadPackedMsg, end, config, ve);

        if (result == 0) {
          // got to end
          return 0;
        } else if (result < 0) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
          on_error();

          if (++numBadTags > MAX_BAD_TAG) {
            NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
            on_error();
            return -1;
          }

          if ((end - *ppReadPackedMsg) >= generic_tl.length) {
            // Advance past the unknown TLV
            (*ppReadPackedMsg) += generic_tl.length + get_tlv_padding(generic_tl.length);
          } else {
            // got ot the dn
            return 0;
          }
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown TAG value: 0x%04x\n", generic_tl.tag);
        on_error();

        if (++numBadTags > MAX_BAD_TAG) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
          on_error();
          return -1;
        }

        if ((end - *ppReadPackedMsg) >= generic_tl.length) {
          // Advance past the unknown TLV
          (*ppReadPackedMsg) += generic_tl.length + get_tlv_padding(generic_tl.length);
        } else {
          // got ot the dn
          return 0;
        }
      }
    }
  }

  return 1;
}

// This intermediate function deals with calculating the length of the value
// and writing into the tlv header.
uint8_t pack_tlv(uint16_t tag, void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end, pack_tlv_fn fn)
{
  nfapi_tl_t *tl = (nfapi_tl_t *)tlv;

  // If the tag is defined
  if (tl->tag == tag) {
    uint8_t *pStartOfTlv = *ppWritePackedMsg;

    // write a dumy tlv header
    if (pack_tl(tl, ppWritePackedMsg, end) == 0)
      return 0;

    // Record the start of the value
    uint8_t *pStartOfValue = *ppWritePackedMsg;

    // pack the tlv value
    if (fn(tlv, ppWritePackedMsg, end) == 0)
      return 0;

    // calculate the length of the value and rewrite the tl header
    tl->length = (*ppWritePackedMsg) - pStartOfValue;
    // rewrite the header with the correct length
    pack_tl(tl, &pStartOfTlv, end);
  } else {
    if (tl->tag != 0) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "Warning pack_tlv tag 0x%x does not match expected 0x%x\n", tl->tag, tag);
    } else {
      // NFAPI_TRACE(NFAPI_TRACE_ERROR, "Warning pack_tlv tag 0x%x ZERO does not match expected 0x%x\n", tl->tag, tag);
    }
  }

  return 1;
}

uint8_t pack_nr_tlv(uint16_t tag, void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end, pack_tlv_fn fn)
{
  nfapi_tl_t *tl = (nfapi_tl_t *)tlv;

  // If the tag is defined
  if (tl->tag == tag) {
    uint8_t *pStartOfTlv = *ppWritePackedMsg;

    // write a dumy tlv header
    if (pack_tl(tl, ppWritePackedMsg, end) == 0)
      return 0;

    // Record the start of the value
    uint8_t *pStartOfValue = *ppWritePackedMsg;

    // pack the tlv value
    if (fn(tlv, ppWritePackedMsg, end) == 0)
      return 0;

    // calculate the length of the value and rewrite the tl header
    tl->length = (*ppWritePackedMsg) - pStartOfValue;
    // rewrite the header with the correct length
    pack_tl(tl, &pStartOfTlv, end);
    // Add padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
    int padding = get_tlv_padding(tl->length);
    NFAPI_TRACE(NFAPI_TRACE_DEBUG, "TLV 0x%x with padding of %d bytes\n", tl->tag, padding);
    if (padding != 0) {
      memset(*ppWritePackedMsg, 0, padding);
      (*ppWritePackedMsg) += padding;
    }
  } else {
    if (tl->tag != 0) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "Warning pack_tlv tag 0x%x does not match expected 0x%x\n", tl->tag, tag);
    } else {
      // NFAPI_TRACE(NFAPI_TRACE_ERROR, "Warning pack_tlv tag 0x%x ZERO does not match expected 0x%x\n", tl->tag, tag);
    }
  }

  return 1;
}

uint8_t pack_nr_generic_tlv(uint16_t tag, void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_nr_generic_tlv_scf_t *toPack = (nfapi_nr_generic_tlv_scf_t *)tlv;
  // If the tag is defined
  if (toPack->tl.tag == tag && toPack->tl.length != 0) {
    // write the tlv header, length comes with value
    if (pack_tl(&(toPack->tl), ppWritePackedMsg, end) == 0)
      return 0;

    // pack the tlv value
    switch (toPack->tl.length) {
      case UINT_8:
        if (push8(toPack->value.u8, ppWritePackedMsg, end) == 0) {
          return 0;
        }
        break;
      case UINT_16:
        if (push16(toPack->value.u16, ppWritePackedMsg, end) == 0) {
          return 0;
        }
        break;
      case UINT_32:
        if (push32(toPack->value.u32, ppWritePackedMsg, end) == 0) {
          return 0;
        }
        break;
      case ARRAY_UINT_16:
        if ((push16(toPack->value.array_u16[0], ppWritePackedMsg, end) && push16(toPack->value.array_u16[1], ppWritePackedMsg, end)
             && push16(toPack->value.array_u16[2], ppWritePackedMsg, end)
             && push16(toPack->value.array_u16[3], ppWritePackedMsg, end)
             && push16(toPack->value.array_u16[4], ppWritePackedMsg, end))
            == 0) {
          return 0;
        }
        break;
      default:
        break;
    }

    // calculate the length of the value and rewrite the tl header
    // in case of nfapi_nr_config_response_tlv_list_scf_t this is to come pre-calculated, possibly unnecessary
    /*toPack->tl.length = (*ppWritePackedMsg) - pStartOfValue;
    // rewrite the header with the correct length
    pack_tl(&(toPack->tl), &pStartOfTlv, end);*/
    // Add padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
    int padding = get_tlv_padding(toPack->tl.length);
    NFAPI_TRACE(NFAPI_TRACE_DEBUG, "TLV 0x%x with padding of %d bytes\n", toPack->tl.tag, padding);
    if (padding != 0) {
      memset(*ppWritePackedMsg, 0, padding);
      (*ppWritePackedMsg) += padding;
    }
  } else {
    if (toPack->tl.tag != 0) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "Warning pack_tlv tag 0x%x does not match expected 0x%x\n", toPack->tl.tag, tag);
    } else {
      // NFAPI_TRACE(NFAPI_TRACE_ERROR, "Warning pack_tlv tag 0x%x ZERO does not match expected 0x%x\n", tl->tag, tag);
    }
  }

  return 1;
}

uint8_t unpack_nr_generic_tlv()
{
  return 1;
}

uint8_t unpack_nr_generic_tlv_list(void *tlv_list, uint8_t tlv_count, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_nr_generic_tlv_scf_t *toUnpack = (nfapi_nr_generic_tlv_scf_t *)tlv_list;
  uint8_t numBadTags = 0;
  for (int i = 0; i < tlv_count; ++i) {
    nfapi_nr_generic_tlv_scf_t *element = &(toUnpack[i]);

    // unpack each generic tlv
    //  unpack the tl and process the values accordingly
    if (unpack_tl(ppReadPackedMsg, &(element->tl), end) == 0)
      return 0;

    uint8_t *pStartOfValue = *ppReadPackedMsg;
    // check for valid tag
    if (element->tl.tag < NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG || element->tl.tag > NFAPI_NR_CONFIG_RSSI_MEASUREMENT_TAG) {
      numBadTags++;
      if (numBadTags > MAX_BAD_TAG) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "Supplied message has had too many bad tags\n");
        on_error();
        return 0;
      }
    }
    // if tag is valid, check length to determine which type to unpack
    switch (element->tl.length) {
      case UINT_8:
        pull8(ppReadPackedMsg, &(element->value.u8), end);
        break;
      case UINT_16:
        pull16(ppReadPackedMsg, &(element->value.u16), end);
        break;
      case UINT_32:
        pull32(ppReadPackedMsg, &(element->value.u32), end);
        break;
      case ARRAY_UINT_16:
        for (int j = 0; j < 5; ++j) {
          pull16(ppReadPackedMsg, &(element->value.array_u16[j]), end);
        }
        break;
      default:
        printf("unknown length %d\n", element->tl.length);
        break;
    }

    // check if the length was right;
    if (element->tl.length != (*ppReadPackedMsg - pStartOfValue)) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR,
                  "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                  element->tl.tag,
                  element->tl.length,
                  (*ppReadPackedMsg - pStartOfValue));
      on_error();
    }
    // Remove padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
    int padding = get_tlv_padding(element->tl.length);
    if (padding != 0) {
      (*ppReadPackedMsg) += padding;
    }
  }
  return 1;
}

const char *nfapi_error_code_to_str(nfapi_error_code_e value)
{
  switch (value) {
    case NFAPI_MSG_OK:
      return "NFAPI_MSG_OK";

    case NFAPI_MSG_INVALID_STATE:
      return "NFAPI_MSG_INVALID_STATE";

    case NFAPI_MSG_INVALID_CONFIG:
      return "NFAPI_MSG_INVALID_CONFIG";

    case NFAPI_SFN_OUT_OF_SYNC:
      return "NFAPI_SFN_OUT_OF_SYNC";

    case NFAPI_MSG_SUBFRAME_ERR:
      return "NFAPI_MSG_SUBFRAME_ERR";

    case NFAPI_MSG_BCH_MISSING:
      return "NFAPI_MSG_BCH_MISSING";

    case NFAPI_MSG_INVALID_SFN:
      return "NFAPI_MSG_INVALID_SFN";

    case NFAPI_MSG_HI_ERR:
      return "NFAPI_MSG_HI_ERR";

    case NFAPI_MSG_TX_ERR:
      return "NFAPI_MSG_TX_ERR";

    default:
      return "UNKNOWN";
  }
}

uint8_t get_tlv_padding(uint16_t tlv_length)
{
  return (4 - (tlv_length % 4)) % 4;
}

uint8_t pack_pnf_param_general_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_pnf_param_general_t *value = (nfapi_pnf_param_general_t *)tlv;
  return push8(value->nfapi_sync_mode, ppWritePackedMsg, end) && push8(value->location_mode, ppWritePackedMsg, end)
         && push16(value->location_coordinates_length, ppWritePackedMsg, end)
         && pusharray8(value->location_coordinates,
                       NFAPI_PNF_PARAM_GENERAL_LOCATION_LENGTH,
                       value->location_coordinates_length,
                       ppWritePackedMsg,
                       end)
         && push32(value->dl_config_timing, ppWritePackedMsg, end) && push32(value->tx_timing, ppWritePackedMsg, end)
         && push32(value->ul_config_timing, ppWritePackedMsg, end) && push32(value->hi_dci0_timing, ppWritePackedMsg, end)
         && push16(value->maximum_number_phys, ppWritePackedMsg, end)
         && push16(value->maximum_total_bandwidth, ppWritePackedMsg, end)
         && push8(value->maximum_total_number_dl_layers, ppWritePackedMsg, end)
         && push8(value->maximum_total_number_ul_layers, ppWritePackedMsg, end) && push8(value->shared_bands, ppWritePackedMsg, end)
         && push8(value->shared_pa, ppWritePackedMsg, end) && pushs16(value->maximum_total_power, ppWritePackedMsg, end)
         && pusharray8(value->oui, NFAPI_PNF_PARAM_GENERAL_OUI_LENGTH, NFAPI_PNF_PARAM_GENERAL_OUI_LENGTH, ppWritePackedMsg, end);
}

uint8_t unpack_pnf_param_general_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_pnf_param_general_t *value = (nfapi_pnf_param_general_t *)tlv;
  return pull8(ppReadPackedMsg, &value->nfapi_sync_mode, end) && pull8(ppReadPackedMsg, &value->location_mode, end)
         && pull16(ppReadPackedMsg, &value->location_coordinates_length, end)
         && pullarray8(ppReadPackedMsg,
                       value->location_coordinates,
                       NFAPI_PNF_PARAM_GENERAL_LOCATION_LENGTH,
                       value->location_coordinates_length,
                       end)
         && pull32(ppReadPackedMsg, &value->dl_config_timing, end) && pull32(ppReadPackedMsg, &value->tx_timing, end)
         && pull32(ppReadPackedMsg, &value->ul_config_timing, end) && pull32(ppReadPackedMsg, &value->hi_dci0_timing, end)
         && pull16(ppReadPackedMsg, &value->maximum_number_phys, end)
         && pull16(ppReadPackedMsg, &value->maximum_total_bandwidth, end)
         && pull8(ppReadPackedMsg, &value->maximum_total_number_dl_layers, end)
         && pull8(ppReadPackedMsg, &value->maximum_total_number_ul_layers, end) && pull8(ppReadPackedMsg, &value->shared_bands, end)
         && pull8(ppReadPackedMsg, &value->shared_pa, end) && pulls16(ppReadPackedMsg, &value->maximum_total_power, end)
         && pullarray8(ppReadPackedMsg, value->oui, NFAPI_PNF_PARAM_GENERAL_OUI_LENGTH, NFAPI_PNF_PARAM_GENERAL_OUI_LENGTH, end);
}

uint8_t pack_rf_config_info(void *elem, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_rf_config_info_t *rf = (nfapi_rf_config_info_t *)elem;
  return push16(rf->rf_config_index, ppWritePackedMsg, end);
}

uint8_t unpack_rf_config_info(void *elem, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_rf_config_info_t *info = (nfapi_rf_config_info_t *)elem;
  return pull16(ppReadPackedMsg, &info->rf_config_index, end);
}

uint8_t pack_pnf_phy_info(void *elem, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_info_t *phy = (nfapi_pnf_phy_info_t *)elem;
  return push16(phy->phy_config_index, ppWritePackedMsg, end) && push16(phy->number_of_rfs, ppWritePackedMsg, end)
         && packarray(phy->rf_config,
                      sizeof(nfapi_rf_config_info_t),
                      NFAPI_MAX_PNF_PHY_RF_CONFIG,
                      phy->number_of_rfs,
                      ppWritePackedMsg,
                      end,
                      &pack_rf_config_info)
         && push16(phy->number_of_rf_exclusions, ppWritePackedMsg, end)
         && packarray(phy->excluded_rf_config,
                      sizeof(nfapi_rf_config_info_t),
                      NFAPI_MAX_PNF_PHY_RF_CONFIG,
                      phy->number_of_rf_exclusions,
                      ppWritePackedMsg,
                      end,
                      &pack_rf_config_info)
         && push16(phy->downlink_channel_bandwidth_supported, ppWritePackedMsg, end)
         && push16(phy->uplink_channel_bandwidth_supported, ppWritePackedMsg, end)
         && push8(phy->number_of_dl_layers_supported, ppWritePackedMsg, end)
         && push8(phy->number_of_ul_layers_supported, ppWritePackedMsg, end)
         && push16(phy->maximum_3gpp_release_supported, ppWritePackedMsg, end)
         && push8(phy->nmm_modes_supported, ppWritePackedMsg, end);
}

uint8_t unpack_pnf_phy_info(void *elem, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_info_t *phy = (nfapi_pnf_phy_info_t *)elem;
  return pull16(ppReadPackedMsg, &phy->phy_config_index, end) && pull16(ppReadPackedMsg, &phy->number_of_rfs, end)
         && unpackarray(ppReadPackedMsg,
                        phy->rf_config,
                        sizeof(nfapi_rf_config_info_t),
                        NFAPI_MAX_PNF_PHY_RF_CONFIG,
                        phy->number_of_rfs,
                        end,
                        &unpack_rf_config_info)
         && pull16(ppReadPackedMsg, &phy->number_of_rf_exclusions, end)
         && unpackarray(ppReadPackedMsg,
                        phy->excluded_rf_config,
                        sizeof(nfapi_rf_config_info_t),
                        NFAPI_MAX_PNF_PHY_RF_CONFIG,
                        phy->number_of_rf_exclusions,
                        end,
                        &unpack_rf_config_info)
         && pull16(ppReadPackedMsg, &phy->downlink_channel_bandwidth_supported, end)
         && pull16(ppReadPackedMsg, &phy->uplink_channel_bandwidth_supported, end)
         && pull8(ppReadPackedMsg, &phy->number_of_dl_layers_supported, end)
         && pull8(ppReadPackedMsg, &phy->number_of_ul_layers_supported, end)
         && pull16(ppReadPackedMsg, &phy->maximum_3gpp_release_supported, end)
         && pull8(ppReadPackedMsg, &phy->nmm_modes_supported, end);
}

uint8_t pack_pnf_phy_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_t *value = (nfapi_pnf_phy_t *)tlv;
  return push16(value->number_of_phys, ppWritePackedMsg, end)
         && packarray(value->phy,
                      sizeof(nfapi_pnf_phy_info_t),
                      NFAPI_MAX_PNF_PHY,
                      value->number_of_phys,
                      ppWritePackedMsg,
                      end,
                      &pack_pnf_phy_info);
}

uint8_t unpack_pnf_phy_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_t *value = (nfapi_pnf_phy_t *)tlv;
  return pull16(ppReadPackedMsg, &value->number_of_phys, end)
         && unpackarray(ppReadPackedMsg,
                        value->phy,
                        sizeof(nfapi_pnf_phy_info_t),
                        NFAPI_MAX_PNF_PHY,
                        value->number_of_phys,
                        end,
                        &unpack_pnf_phy_info);
}

uint8_t pack_phy_rf_config_info(void *elem, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_phy_rf_config_info_t *rf = (nfapi_phy_rf_config_info_t *)elem;
  return push16(rf->phy_id, ppWritePackedMsg, end) && push16(rf->phy_config_index, ppWritePackedMsg, end)
         && push16(rf->rf_config_index, ppWritePackedMsg, end);
}

uint8_t unpack_phy_rf_config_info(void *elem, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_phy_rf_config_info_t *rf = (nfapi_phy_rf_config_info_t *)elem;
  return pull16(ppReadPackedMsg, &rf->phy_id, end) && pull16(ppReadPackedMsg, &rf->phy_config_index, end)
         && pull16(ppReadPackedMsg, &rf->rf_config_index, end);
}

uint8_t pack_pnf_phy_rf_config_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_rf_config_t *value = (nfapi_pnf_phy_rf_config_t *)tlv;
  return push16(value->number_phy_rf_config_info, ppWritePackedMsg, end)
         && packarray(value->phy_rf_config,
                      sizeof(nfapi_phy_rf_config_info_t),
                      NFAPI_MAX_PHY_RF_INSTANCES,
                      value->number_phy_rf_config_info,
                      ppWritePackedMsg,
                      end,
                      &pack_phy_rf_config_info);
}

uint8_t unpack_pnf_phy_rf_config_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_pnf_phy_rf_config_t *value = (nfapi_pnf_phy_rf_config_t *)tlv;
  return pull16(ppReadPackedMsg, &value->number_phy_rf_config_info, end)
         && unpackarray(ppReadPackedMsg,
                        value->phy_rf_config,
                        sizeof(nfapi_phy_rf_config_info_t),
                        NFAPI_MAX_PHY_RF_INSTANCES,
                        value->number_phy_rf_config_info,
                        end,
                        &unpack_phy_rf_config_info);
}

uint8_t pack_ipv4_address_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_ipv4_address_t *value = (nfapi_ipv4_address_t *)tlv;
  return pusharray8(value->address, NFAPI_IPV4_ADDRESS_LENGTH, NFAPI_IPV4_ADDRESS_LENGTH, ppWritePackedMsg, end);
}

uint8_t unpack_ipv4_address_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_ipv4_address_t *value = (nfapi_ipv4_address_t *)tlv;
  return pullarray8(ppReadPackedMsg, value->address, NFAPI_IPV4_ADDRESS_LENGTH, NFAPI_IPV4_ADDRESS_LENGTH, end);
}

uint8_t pack_ipv6_address_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_ipv6_address_t *value = (nfapi_ipv6_address_t *)tlv;
  return pusharray8(value->address, NFAPI_IPV6_ADDRESS_LENGTH, NFAPI_IPV6_ADDRESS_LENGTH, ppWritePackedMsg, end);
}

uint8_t unpack_ipv6_address_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_ipv4_address_t *value = (nfapi_ipv4_address_t *)tlv;
  return pullarray8(ppReadPackedMsg, value->address, NFAPI_IPV6_ADDRESS_LENGTH, NFAPI_IPV6_ADDRESS_LENGTH, end);
}

uint8_t pack_stop_response(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *config)
{
  nfapi_stop_response_t *pNfapiMsg = (nfapi_stop_response_t *)msg;
  return push32(pNfapiMsg->error_code, ppWritePackedMsg, end)
         && pack_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config);
}

uint8_t unpack_stop_response(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p4_p5_codec_config_t *config)
{
  nfapi_stop_response_t *pNfapiMsg = (nfapi_stop_response_t *)msg;
  return pull32(ppReadPackedMsg, &pNfapiMsg->error_code, end)
         && unpack_tlv_list(NULL, 0, ppReadPackedMsg, end, config, &(pNfapiMsg->vendor_extension));
}

uint8_t pack_uint32_tlv_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_uint32_tlv_t *value = (nfapi_uint32_tlv_t *)tlv;
  return push32(value->value, ppWritePackedMsg, end);
}

uint8_t unpack_uint32_tlv_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_uint32_tlv_t *value = (nfapi_uint32_tlv_t *)tlv;
  return pull32(ppReadPackedMsg, &value->value, end);
}

uint8_t pack_uint16_tlv_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_uint16_tlv_t *value = (nfapi_uint16_tlv_t *)tlv;
  return push16(value->value, ppWritePackedMsg, end);
}

uint8_t unpack_uint16_tlv_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_uint16_tlv_t *value = (nfapi_uint16_tlv_t *)tlv;
  return pull16(ppReadPackedMsg, &value->value, end);
}

uint8_t pack_int16_tlv_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_int16_tlv_t *value = (nfapi_int16_tlv_t *)tlv;
  return pushs16(value->value, ppWritePackedMsg, end);
}

uint8_t unpack_int16_tlv_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_int16_tlv_t *value = (nfapi_int16_tlv_t *)tlv;
  return pulls16(ppReadPackedMsg, &value->value, end);
}

uint8_t pack_uint8_tlv_value(void *tlv, uint8_t **ppWritePackedMsg, uint8_t *end)
{
  nfapi_uint8_tlv_t *value = (nfapi_uint8_tlv_t *)tlv;
  return push8(value->value, ppWritePackedMsg, end);
}

uint8_t unpack_uint8_tlv_value(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  nfapi_uint8_tlv_t *value = (nfapi_uint8_tlv_t *)tlv;
  return pull8(ppReadPackedMsg, &value->value, end);
}

// helper function for message length calculation -
// takes the pointers to the start of message to end of message
uint32_t get_packed_msg_len(uintptr_t msgHead, uintptr_t msgEnd)
{
  if (msgEnd < msgHead) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "get_packed_msg_len: Error in pointers supplied %p, %p\n", &msgHead, &msgEnd);
    return 0;
  }

  return msgEnd - msgHead;
}
