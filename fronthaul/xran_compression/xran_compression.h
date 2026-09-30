/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#ifndef _XRAN_COMPRESSION_H_
#define _XRAN_COMPRESSION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xranlib_compress_request {
  int16_t *data_in; /*!< Pointer to data to compress. */
  int16_t numRBs; /*!< numRBs  */
  int16_t numDataElements; /*!< number of elements in block process [UP: 24 i.e 12RE*2; CP: 16,32,64,128. i.e AntElm*2] */
  int16_t compMethod; /*!< Compression method */
  int16_t iqWidth; /*!< Bit size */
  int16_t reMask; /*!< 12-bit RE mask representing 12REs in one RB  */
  int16_t csf; /*!< 1-bit constellation shift flag defined in section 5.4.7.4  */
  uint16_t ScaleFactor; /*!< Scale factor as defined in section A.5*/
  int32_t len; /*!< Length of input buffer in bytes */
};

struct xranlib_compress_response {
  int8_t *data_out; /*!< Pointer to data after compression. */
  int32_t len; /*!< Length of output data. */
};

struct xranlib_decompress_request {
  int8_t *data_in; /*!< Pointer to data to decompress. */
  int16_t numRBs; /*!< numRBs  */
  int16_t numDataElements; /*!< number of elements in block process [UP: 24 i.e 12RE*2; CP: 16,32,64,128. i.e AntElm*2] */
  int16_t compMethod; /*!< Compression method */
  int16_t iqWidth; /*!< Bit size */
  int16_t reMask; /*!< 12-bit RE mask representing 12REs in one RB  */
  int16_t csf; /*!< 1-bit constellation shift flag defined in section 5.4.7.4  */
  uint16_t ScaleFactor; /*!< Scale factor as defined in section A.5*/
  int32_t len; /*!< Length of input data. */
  int16_t SprEnable; /*!< whether enable spr data cvt int16 to fp16 ,0 - disable/1 - enable */
  float fScale; /*!< Scale of the spr data cvt */
};


struct xranlib_decompress_response {
  int16_t *data_out; /*!< Pointer to data after decompression. */
  int32_t len; /*!< Length of output data. */
};

int32_t xranlib_compress(const struct xranlib_compress_request *request, struct xranlib_compress_response *response);
int32_t xranlib_compress_sse(const struct xranlib_compress_request *request, struct xranlib_compress_response *response);
int32_t xranlib_compress_avx2(const struct xranlib_compress_request *request, struct xranlib_compress_response *response);
int32_t xranlib_compress_avx512(const struct xranlib_compress_request *request, struct xranlib_compress_response *response);
int32_t xranlib_decompress(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response);
int32_t xranlib_decompress_sse(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response);
int32_t xranlib_decompress_avx2(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response);
int32_t xranlib_decompress_avx512(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response);

#ifdef __cplusplus
}
#endif

#endif /* _XRAN_COMPRESSION_H_ */
