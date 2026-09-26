/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E3_CONFIG_H
#define E3_CONFIG_H

#include <stdint.h>

/* SM identifiers, used by enabled_sms */
#define E3_SM_ID_SPECTRUM 1 /* Spectrum SM: sensing telemetry + control */
#define E3_SM_ID_KPM 2 /* L1-KPM SM: PHY IQ metadata */

/* link_layer values (match libe3 e3_config_t; -1 = libe3 default) */
#define E3_LINK_ZMQ 0
#define E3_LINK_POSIX 1

/* transport_layer values (match libe3 e3_config_t; -1 = libe3 default) */
#define E3_TRANSPORT_SCTP 0
#define E3_TRANSPORT_TCP 1
#define E3_TRANSPORT_IPC 2

/* encoding values (match libe3 e3_config_t; -1 = libe3 default) */
#define E3_ENCODING_ASN1 0
#define E3_ENCODING_JSON 1
#define E3_ENCODING_PROTOBUF 2

typedef struct {
  int link_layer; /* E3_LINK_{ZMQ,POSIX}; -1 = libe3 default */
  int transport_layer; /* E3_TRANSPORT_{SCTP,TCP,IPC}; -1 = libe3 default */
  int encoding; /* E3_ENCODING_{ASN1,JSON,PROTOBUF}; -1 = libe3 default */
  uint16_t setup_port; /* dApp setup REP port; 0 = libe3 default (9990) */
  uint16_t subscriber_port; /* dApp command SUB port; 0 = libe3 default */
  uint16_t publisher_port; /* dApp indication PUB port; 0 = libe3 default */
  int32_t *enabled_sms; /* SM IDs to enable (NULL/empty = enable all) */
  int num_enabled_sms; /* number of entries in enabled_sms */
} e3_cmdline_config_t;

/**
 * @brief Read E3 configuration from the OAI config file
 * @param config Pointer to configuration structure to fill
 */
void e3_readconfig(e3_cmdline_config_t *config);

#endif // E3_CONFIG_H
