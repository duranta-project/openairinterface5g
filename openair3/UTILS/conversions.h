/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef CONVERSIONS_H_
#define CONVERSIONS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "assertions.h"

/* The conversions below work on little and big endian hosts: the byte order comes from the compiler
 * (__BYTE_ORDER__, __builtin_bswap*()), the memory is accessed with memcpy() only. */
#if !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ && __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error "unknown host byte order"
#endif

/* host order to/from big endian (network order), the same operation both ways */
static inline uint64_t conv_be64(uint64_t v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return __builtin_bswap64(v);
#else
  return v;
#endif
}

static inline uint32_t conv_be32(uint32_t v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return __builtin_bswap32(v);
#else
  return v;
#endif
}

/* Big endian (network order) write of the n (1 to 8) least significant bytes of v */
static inline void conv_put_be(uint8_t *buf, uint64_t v, int n)
{
  const uint64_t be = conv_be64(v);
  memcpy(buf, (const uint8_t *)&be + sizeof(be) - n, n);
}

/* Big endian (network order) read of n (1 to 8) bytes */
static inline uint64_t conv_get_be(const uint8_t *buf, int n)
{
  uint64_t be = 0;
  memcpy((uint8_t *)&be + sizeof(be) - n, buf, n);
  return conv_be64(be);
}

static inline uint8_t *conv_calloc(size_t n)
{
  uint8_t *p = (uint8_t *)calloc(n, 1);
  AssertFatal(p != NULL, "calloc(%zu) failed\n", n);
  return p;
}

static inline uint32_t ntoh_int32_buf(const void *buf)
{
  uint32_t n;
  memcpy(&n, buf, sizeof(n));
  return conv_be32(n);
}

static inline uint32_t ntoh_int24_buf(const void *buf)
{
  return (uint32_t)conv_get_be((const uint8_t *)buf, 3);
}

/* Integer to/from a big endian buffer */
#define INT16_TO_BUFFER(x, buf) conv_put_be((uint8_t *)(buf), (uint64_t)(x), 2)
#define INT24_TO_BUFFER(x, buf) conv_put_be((uint8_t *)(buf), (uint64_t)(x), 3)
#define INT32_TO_BUFFER(x, buf) conv_put_be((uint8_t *)(buf), (uint64_t)(x), 4)
#define BUFFER_TO_INT24(buf, x) ((x) = (uint32_t)conv_get_be((const uint8_t *)(buf), 3))
#define BUFFER_TO_UINT32(buf, x) ((x) = (uint32_t)conv_get_be((const uint8_t *)(buf), 4))

/* Allocate n bytes in an asn1c OCTET STRING, BIT STRING or INTEGER and store v big endian.
 * The ASN.1 types differ, hence macros: aSN is a pointer evaluated several times, v once. */
#define CONV_TO_ASN_BUF(aSN, n, v)          \
  do {                                      \
    const uint64_t _conv_v = (uint64_t)(v); \
    (aSN)->buf = conv_calloc(n);            \
    (aSN)->size = (n);                      \
    conv_put_be((aSN)->buf, _conv_v, (n));  \
  } while (0)

/* BIT STRING of n bytes holding the value v left aligned, with unused trailing bits */
#define CONV_TO_BIT_STRING(aSN, n, unused, v)           \
  do {                                                  \
    CONV_TO_ASN_BUF(aSN, n, (uint64_t)(v) << (unused)); \
    (aSN)->bits_unused = (unused);                      \
  } while (0)

#define INT8_TO_OCTET_STRING(x, aSN) CONV_TO_ASN_BUF(aSN, 1, x)
#define INT16_TO_OCTET_STRING(x, aSN) CONV_TO_ASN_BUF(aSN, 2, x)
#define INT24_TO_OCTET_STRING(x, aSN) CONV_TO_ASN_BUF(aSN, 3, x)
#define INT32_TO_OCTET_STRING(x, aSN) CONV_TO_ASN_BUF(aSN, 4, x)

#define INT16_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 2, 0, x)
#define INT24_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 3, 0, x)
#define INT32_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 4, 0, x)

#define MME_CODE_TO_OCTET_STRING INT8_TO_OCTET_STRING
#define M_TMSI_TO_OCTET_STRING INT32_TO_OCTET_STRING
#define MME_GID_TO_OCTET_STRING INT16_TO_OCTET_STRING
#define TAC_TO_ASN1 INT16_TO_OCTET_STRING
#define GTP_TEID_TO_ASN1 INT32_TO_OCTET_STRING

/* TS 38.413 9.3.3.12: AMF Region ID, BIT STRING (8) */
#define AMF_REGION_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 1, 0, x)
/* TS 38.413 9.3.3.12: AMF Set ID, BIT STRING (10) */
#define AMF_SETID_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 2, 6, (x) & 0x3ff)
/* TS 38.413 9.3.3.12: AMF Pointer, BIT STRING (6) */
#define AMF_POINTER_TO_BIT_STRING(x, aSN) CONV_TO_BIT_STRING(aSN, 1, 2, (x) & 0x3f)

#define ENCRALG_TO_BIT_STRING(encralg, bitstring) CONV_TO_BIT_STRING(bitstring, 2, 0, encralg)
#define INTPROTALG_TO_BIT_STRING(intprotalg, bitstring) CONV_TO_BIT_STRING(bitstring, 2, 0, intprotalg)

#define AS_KEY_STAR_TO_BIT_STRING(askeystar, bitstring) \
  do {                                                  \
    (bitstring)->buf = conv_calloc(32);                 \
    memcpy((bitstring)->buf, (askeystar), 32);          \
    (bitstring)->size = 32;                             \
    (bitstring)->bits_unused = 0;                       \
  } while (0)

/* UE aggregate maximum bit rate: INTEGER on 5 bytes */
#define UEAGMAXBITRTD_TO_ASN_PRIMITIVES(uegmaxbitrtd, asnprimitives) CONV_TO_ASN_BUF(asnprimitives, 5, uegmaxbitrtd)
#define UEAGMAXBITRTU_TO_ASN_PRIMITIVES(uegmaxbitrtu, asnprimitives) CONV_TO_ASN_BUF(asnprimitives, 5, uegmaxbitrtu)

/* ASN.1 OCTET STRING to integer (x is an lvalue) */
#define OCTET_STRING_TO_INT8(aSN, x)               \
  do {                                             \
    DevCheck((aSN)->size == 1, (aSN)->size, 0, 0); \
    (x) = (aSN)->buf[0];                           \
  } while (0)

/* reads the 2 first bytes, also of a 3 bytes string */
#define OCTET_STRING_TO_INT16(aSN, x)                                  \
  do {                                                                 \
    DevCheck((aSN)->size == 2 || (aSN)->size == 3, (aSN)->size, 0, 0); \
    (x) = (uint16_t)conv_get_be((aSN)->buf, 2);                        \
  } while (0)

#define OCTET_STRING_TO_INT24(aSN, x)                                  \
  do {                                                                 \
    DevCheck((aSN)->size == 2 || (aSN)->size == 3, (aSN)->size, 0, 0); \
    (x) = (uint32_t)conv_get_be((aSN)->buf, (int)(aSN)->size);         \
  } while (0)

#define OCTET_STRING_TO_UINT32(aSN, x)             \
  do {                                             \
    DevCheck((aSN)->size == 4, (aSN)->size, 0, 0); \
    (x) = (uint32_t)conv_get_be((aSN)->buf, 4);    \
  } while (0)
#define OCTET_STRING_TO_INT32 OCTET_STRING_TO_UINT32

/* TS 38.473 9.3.1.12: NR Cell Identity, BIT STRING (36) */
#define NR_CELL_ID_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 5, 4, mACRO)

#define BIT_STRING_TO_NR_CELL_IDENTITY(aSN, vALUE)               \
  do {                                                           \
    DevCheck((aSN)->bits_unused == 4, (aSN)->bits_unused, 4, 0); \
    DevCheck((aSN)->size == 5, (aSN)->size, 5, 0);               \
    (vALUE) = conv_get_be((aSN)->buf, 5) >> 4;                   \
  } while (0)

/* PLMN identity: MCC and MNC digits in TBCD, MNC hundreds set to 0xf for 2 digit MNCs */
#define MCC_HUNDREDS(vALUE) ((vALUE) / 100)
#define MNC_HUNDREDS(vALUE, mNCdIGITlENGTH) ((mNCdIGITlENGTH) == 2 ? 15 : (vALUE) / 100)
#define MCC_MNC_DECIMAL(vALUE) (((vALUE) / 10) % 10)
#define MCC_MNC_DIGIT(vALUE) ((vALUE) % 10)

static inline void conv_mcc_mnc_to_tbcd(int mcc, int mnc, int mnc_digit_length, uint8_t out[3])
{
  out[0] = (uint8_t)((MCC_MNC_DECIMAL(mcc) << 4) | MCC_HUNDREDS(mcc));
  out[1] = (uint8_t)((MNC_HUNDREDS(mnc, mnc_digit_length) << 4) | MCC_MNC_DIGIT(mcc));
  out[2] = (uint8_t)((MCC_MNC_DIGIT(mnc) << 4) | MCC_MNC_DECIMAL(mnc));
}

#define MCC_MNC_TO_PLMNID(mCC, mNC, mNCdIGITlENGTH, oCTETsTRING)              \
  do {                                                                        \
    (oCTETsTRING)->buf = conv_calloc(3);                                      \
    conv_mcc_mnc_to_tbcd((mCC), (mNC), (mNCdIGITlENGTH), (oCTETsTRING)->buf); \
    (oCTETsTRING)->size = 3;                                                  \
  } while (0)

#define MCC_MNC_TO_TBCD(mCC, mNC, mNCdIGITlENGTH, tBCDsTRING)        \
  do {                                                               \
    uint8_t _conv_buf[3];                                            \
    DevAssert((mNCdIGITlENGTH) == 3 || (mNCdIGITlENGTH) == 2);       \
    conv_mcc_mnc_to_tbcd((mCC), (mNC), (mNCdIGITlENGTH), _conv_buf); \
    OCTET_STRING_fromBuf((tBCDsTRING), (const char *)_conv_buf, 3);  \
  } while (0)

/* mCC, mNC and mNCdIGITlENGTH are lvalues */
#define PLMNID_TO_MCC_MNC(oCTETsTRING, mCC, mNC, mNCdIGITlENGTH)                                                \
  do {                                                                                                          \
    const uint8_t *_conv_b = (oCTETsTRING)->buf;                                                                \
    const int _conv_mnc3 = (_conv_b[1] >> 4) & 0x0F;                                                            \
    (mCC) = (_conv_b[0] & 0x0F) * 100 + ((_conv_b[0] >> 4) & 0x0F) * 10 + (_conv_b[1] & 0x0F);                  \
    (mNCdIGITlENGTH) = _conv_mnc3 == 0xF ? 2 : 3;                                                               \
    (mNC) = (_conv_mnc3 == 0xF ? 0 : _conv_mnc3 * 100) + (_conv_b[2] & 0x0F) * 10 + ((_conv_b[2] >> 4) & 0x0F); \
  } while (0)

#define TBCD_TO_MCC_MNC(tBCDsTRING, mCC, mNC, mNCdIGITlENGTH) \
  do {                                                        \
    DevAssert((tBCDsTRING)->size == 3);                       \
    PLMNID_TO_MCC_MNC(tBCDsTRING, mCC, mNC, mNCdIGITlENGTH);  \
  } while (0)

/* TS 38.473 9.3.2.3: Transport Layer Address, BIT STRING (32) for IPv4.
 * The address is an uint32_t in network order (in_addr_t, in_addr.s_addr): its bytes are already in the
 * order of the BIT STRING on any host, so they are copied as they are in memory. */
#define TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(mACRO, bITsTRING) \
  do {                                                               \
    const uint32_t _conv_a = (uint32_t)(mACRO);                      \
    (bITsTRING)->buf = conv_calloc(4);                               \
    memcpy((bITsTRING)->buf, &_conv_a, 4);                           \
    (bITsTRING)->size = 4;                                           \
    (bITsTRING)->bits_unused = 0;                                    \
  } while (0)

#define BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(bITsTRING, mACRO)         \
  do {                                                                       \
    uint32_t _conv_a;                                                        \
    DevCheck((bITsTRING)->size == 4, (bITsTRING)->size, 4, 0);               \
    DevCheck((bITsTRING)->bits_unused == 0, (bITsTRING)->bits_unused, 0, 0); \
    memcpy(&_conv_a, (bITsTRING)->buf, 4);                                   \
    (mACRO) = _conv_a;                                                       \
  } while (0)

/* TS 38.413 9.3.3.18: 5GS TAC, OCTET STRING (3) */
#define NR_FIVEGS_TAC_ID_TO_BIT_STRING(x, aSN) CONV_TO_ASN_BUF(aSN, 3, (x) & 0xffff)

/* TS 36.413 9.2.1.37: Macro eNB ID, BIT STRING (20) */
#define MACRO_ENB_ID_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 3, 4, (mACRO) & 0xfffff)

/* gNB ID in a BIT STRING (28) */
#define MACRO_GNB_ID_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 4, 4, (mACRO) & 0xfffffff)

#define MACRO_BIT_STRING_TO_GNB_ID(bITsTRING, oUT) ((oUT) = (uint32_t)(conv_get_be((bITsTRING)->buf, 4) >> 4))

/* TS 36.413 9.2.1.38: E-UTRAN Cell Identity, BIT STRING (28): eNB ID (20 bits) then cell ID (8 bits) */
#define MACRO_ENB_ID_TO_CELL_IDENTITY(mACRO, cELL_iD, bITsTRING) \
  CONV_TO_BIT_STRING(bITsTRING, 4, 4, ((uint64_t)((mACRO) & 0xfffff) << 8) | ((cELL_iD) & 0xff))

/* NR Cell Identity, BIT STRING (36): gNB ID (28 bits) then cell ID (8 bits) */
#define MACRO_GNB_ID_TO_CELL_IDENTITY(mACRO, cELL_iD, bITsTRING) \
  CONV_TO_BIT_STRING(bITsTRING, 5, 4, ((uint64_t)((mACRO) & 0xfffffff) << 8) | ((cELL_iD) & 0xff))

/* TS 38.473 9.3.1.67: UE Identity Index value, BIT STRING (10) */
#define UEIDENTITYINDEX_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 2, 6, (mACRO) & 0x3ff)

/* TS 38.413 9.3.3.20: 5G-S-TMSI, BIT STRING (48) */
#define FIVEG_S_TMSI_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 6, 0, mACRO)

/* TS 38.473 9.3.1.149: I-RNTI, BIT STRING (40) */
#define IRNTI_TO_BIT_STRING(mACRO, bITsTRING) CONV_TO_BIT_STRING(bITsTRING, 5, 0, mACRO)

/* printf() format and arguments of an IPv4 address given as an uint32_t in network order (in_addr_t,
 * in_addr.s_addr), on any host */
#define IPV4_ADDR "%u.%u.%u.%u"
#define IPV4_ADDR_FORMAT(aDDRESS)                                                      \
  (unsigned)(conv_be32(aDDRESS) >> 24), (unsigned)((conv_be32(aDDRESS) >> 16) & 0xff), \
      (unsigned)((conv_be32(aDDRESS) >> 8) & 0xff), (unsigned)(conv_be32(aDDRESS) & 0xff)

#endif /* CONVERSIONS_H_ */
