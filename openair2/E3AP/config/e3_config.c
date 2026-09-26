/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "e3_config.h"

#include <stdbool.h>

#include "../e3_log.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_paramdesc.h"
#include "common/config/config_userapi.h"

#define E3CONFIG_SECTION "E3Configuration"

/* Which E3AP encoders the linked libe3 was built with. The macros reach us from
 * libe3.pc, as CLIBE3_COMPILE_DEFS in openair2/E3AP/CMakeLists.txt. Selecting an
 * encoding libe3 lacks otherwise surfaces only as a null encoder inside libe3,
 * reported as a generic agent-init failure that names nothing. */
#ifdef LIBE3_ENABLE_ASN1
#define E3_ENC_ASN1_NAME " asn1"
#else
#define E3_ENC_ASN1_NAME ""
#endif
#ifdef LIBE3_ENABLE_JSON
#define E3_ENC_JSON_NAME " json"
#else
#define E3_ENC_JSON_NAME ""
#endif
#ifdef LIBE3_ENABLE_PROTOBUF
#define E3_ENC_PROTOBUF_NAME " protobuf"
#else
#define E3_ENC_PROTOBUF_NAME ""
#endif
#define E3_AVAILABLE_ENCODINGS E3_ENC_ASN1_NAME E3_ENC_JSON_NAME E3_ENC_PROTOBUF_NAME

/**
 * @brief Whether the linked libe3 can serve the given E3AP wire encoding.
 * @param encoding One of the E3_ENCODING_* values.
 * @return true if libe3 was built with the matching encoder.
 */
static bool e3_encoding_available(int encoding)
{
  switch (encoding) {
    case E3_ENCODING_ASN1:
#ifdef LIBE3_ENABLE_ASN1
      return true;
#else
      return false;
#endif
    case E3_ENCODING_JSON:
#ifdef LIBE3_ENABLE_JSON
      return true;
#else
      return false;
#endif
    case E3_ENCODING_PROTOBUF:
#ifdef LIBE3_ENABLE_PROTOBUF
      return true;
#else
      return false;
#endif
    default:
      return false;
  }
}

#define simOpt PARAMFLAG_NOFREE | PARAMFLAG_CMDLINE_NOPREFIXENABLED

/* String-to-integer mappings for link, transport, and encoding.
 * Values must match the libe3 e3_config_t enum ordering. */
// clang-format off
#define E3_LINK_OKSTRINGS      {"zmq",  "posix"}
#define E3_LINK_VALUES         {E3_LINK_ZMQ, E3_LINK_POSIX}
#define E3_TRANSPORT_OKSTRINGS {"sctp", "tcp", "ipc"}
#define E3_TRANSPORT_VALUES    {E3_TRANSPORT_SCTP, E3_TRANSPORT_TCP, E3_TRANSPORT_IPC}
#define E3_ENCODING_OKSTRINGS  {"asn1", "json", "protobuf"}
#define E3_ENCODING_VALUES     {E3_ENCODING_ASN1, E3_ENCODING_JSON, E3_ENCODING_PROTOBUF}

#define E3_LINK_IDX        0
#define E3_TRANSPORT_IDX   1
#define E3_ENCODING_IDX    2
#define E3_ENABLED_SMS_IDX 6
// clang-format on

void e3_readconfig(e3_cmdline_config_t *config)
{
  /* Temporary string storage for the string→int parameters. */
  char *s_link = NULL, *s_transport = NULL, *s_encoding = NULL;

  // clang-format off
  paramdef_t e3_params[] = {
    /* optname             helpstr                                   paramflags  value                           defval      type          numelt */
    {"link",              "Link layer for E3 (zmq|posix)",           simOpt, .strptr  = &s_link,               .defstrval = "posix", TYPE_STRING,  0},
    {"transport",         "Transport layer for E3 (sctp|tcp|ipc)",   simOpt, .strptr  = &s_transport,          .defstrval = "ipc",   TYPE_STRING,  0},
    {"encoding",          "Encoding format for E3 (asn1|json|protobuf)", simOpt, .strptr  = &s_encoding,       .defstrval = "asn1",  TYPE_STRING,  0},
    {"setup_port",        "E3 setup port (0=libe3 default)",         simOpt, .u16ptr  = &config->setup_port,      .defuintval = 0, TYPE_UINT16, 0},
    {"subscriber_port",   "E3 subscriber port (0=libe3 default)",    simOpt, .u16ptr  = &config->subscriber_port, .defuintval = 0, TYPE_UINT16, 0},
    {"publisher_port",    "E3 publisher port (0=libe3 default)",     simOpt, .u16ptr  = &config->publisher_port,  .defuintval = 0, TYPE_UINT16, 0},
    {"enabled_sms",       "SM IDs to register (empty=all)",          simOpt, .iptr    = NULL,                  .defintarrayval = NULL, TYPE_INTARRAY, 0},
  };

  checkedparam_t e3_checks[] = {
    {.s3a = {config_checkstr_assign_integer, E3_LINK_OKSTRINGS,      E3_LINK_VALUES,      2}},
    {.s3a = {config_checkstr_assign_integer, E3_TRANSPORT_OKSTRINGS, E3_TRANSPORT_VALUES, 3}},
    {.s3a = {config_checkstr_assign_integer, E3_ENCODING_OKSTRINGS,  E3_ENCODING_VALUES,  3}},
    {.s5 = {NULL}},
    {.s5 = {NULL}},
    {.s5 = {NULL}},
    {.s5 = {NULL}},
  };
  // clang-format on

  config_set_checkfunctions(e3_params, e3_checks, sizeofArray(e3_params));

  int ret = config_get(config_get_if(), e3_params, sizeofArray(e3_params), E3CONFIG_SECTION);
  AssertFatal(ret >= 0, "E3Configuration: config_get failed\n");

  config->link_layer = config_get_processedint(config_get_if(), &e3_params[E3_LINK_IDX]);
  config->transport_layer = config_get_processedint(config_get_if(), &e3_params[E3_TRANSPORT_IDX]);
  config->encoding = config_get_processedint(config_get_if(), &e3_params[E3_ENCODING_IDX]);
  AssertFatal(e3_encoding_available(config->encoding),
              "E3Configuration.encoding=\"%s\" is not compiled into the linked libe3, which "
              "provides:%s. Set encoding to one of those, or rebuild libe3 with the matching "
              "-DLIBE3_ENABLE_<ENCODING>=ON\n",
              s_encoding ? s_encoding : "asn1",
              E3_AVAILABLE_ENCODINGS[0] ? E3_AVAILABLE_ENCODINGS : " (none)");
  /* setup_port, subscriber_port, publisher_port written directly by config_get via u16ptr */

  config->enabled_sms = e3_params[E3_ENABLED_SMS_IDX].iptr;
  config->num_enabled_sms = e3_params[E3_ENABLED_SMS_IDX].numelt;

  E3_LOG_I("E3 configuration: link=%d transport=%d encoding=%d setup_port=%u subscriber_port=%u publisher_port=%u\n",
           config->link_layer,
           config->transport_layer,
           config->encoding,
           config->setup_port,
           config->subscriber_port,
           config->publisher_port);
}
