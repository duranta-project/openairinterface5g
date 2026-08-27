/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "common/utils/LOG/log.h"
#include "common/utils/ds/byte_array.h"
#include "RegistrationAccept.h"
#include "fgs_service_request.h"
#include "fgmm_service_accept.h"
#include "fgmm_service_reject.h"
#include "fgmm_registration_reject.h"
#include "fgmm_authentication_failure.h"
#include "FGSNASSecurityModeReject.h"
#include "fgmm_authentication_reject.h"
#include "nr_nas_msg.h"

configmodule_interface_t *uniqCfg = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  UNUSED(assert);
  printf("detected error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

/**
 * @brief Equality check for NAS Service Request enc/dec
 */
/** @brief Test regression of NAS Registration Accept decoding
 *         This test verifies that the NAS Registration Accept message
 *         is correctly decoded. It decodes a message as it received from
 *         the OAI CN5G and compares the output with the expected Registration Accept structure.
 *         The purpose of this regression test is to ensure that changes
 *         to the NAS decoder do not break the existing decoding behavior with OAI CN5G */
static void test_regression_registration_accept(void)
{
  // Registration Accept message
  registration_accept_msg orig = {
      .guti = malloc_or_fail(sizeof(*orig.guti)),
      .result = 0x01,
      .nas_allowed_nssai[0].sst = 0x01,
      .nas_allowed_nssai[1].sst = 0x02,
      .nas_allowed_nssai[2].sst = 0x03,
      .num_allowed_slices = 3,
      .config_nssai[0].sst = 0x01,
      .config_nssai[1].sst = 0x02,
      .config_nssai[2].sst = 0x03,
      .num_configured_slices = 3,
  };

  orig.guti->guti.typeofidentity = FGS_MOBILE_IDENTITY_5G_GUTI;
  orig.guti->guti.amfpointer = 0x01;
  orig.guti->guti.amfregionid = 0x02;
  orig.guti->guti.amfsetid = 0x01;
  orig.guti->guti.mccdigit1 = 0x00;
  orig.guti->guti.mccdigit2 = 0x00;
  orig.guti->guti.mccdigit3 = 0x01;
  orig.guti->guti.mncdigit1 = 0x00;
  orig.guti->guti.mncdigit2 = 0x01;
  orig.guti->guti.mncdigit3 = 0x0F;
  orig.guti->guti.tmsi = 0xb0207806;
  orig.guti->guti.spare = 0b1111;
  orig.guti->guti.oddeven = 0;

  // Encoded Registration Accept message from OAI CN5G
  uint8_t cn5g_msg[] = {0x01, 0x01, // Registration Result
                        0x77, 0x00, 0x0B, 0xF2, 0x00, 0xF1, 0x10, // 5G-GUTI
                        0x02, 0x00, 0x41, 0xB0, 0x20, 0x78, 0x06, // 5G-GUTI
                        0x15, 0x06, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, // Allowed N-NSSAI
                        0x31, 0x06, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03}; // Configured N-NSSAI

  // Decode NAS Registration Accept
  int encoded_length;
  registration_accept_msg decoded = {0};
  byte_array_t ba = {.buf = cn5g_msg, .len = sizeof(cn5g_msg)};
  encoded_length = decode_registration_accept(&decoded, ba);
  AssertFatal(encoded_length >= 0, "encode_fgs_service_request() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  bool ret = eq_fgmm_registration_accept(&orig, &decoded);
  AssertFatal(ret, "Encoding mismatch!\n");

  // Free dynamic allocated memory
  free_fgmm_registration_accept(&orig);
  free_fgmm_registration_accept(&decoded);
}

/**
 * @brief Test NAS Service Request enc/dec
 */
static void test_service_request(void)
{
  // Dummy NAS Service Request message
  uint16_t amf_set_id = 0x48;
  uint16_t amf_pointer = 0x34;
  uint32_t tmsi = 0x56789ABC;
  uint8_t container_data[] = {0xde, 0xad, 0xbe};
  fgs_service_request_msg_t original_msg = {
      .naskeysetidentifier = {.tsc = NAS_KEY_SET_IDENTIFIER_NATIVE, .naskeysetidentifier = NAS_KEY_SET_IDENTIFIER_NOT_AVAILABLE},
      .serviceType = SERVICE_TYPE_DATA,
      .fiveg_s_tmsi = {.digit1 = DIGIT1,
                       .typeofidentity = FGS_MOBILE_IDENTITY_5GS_TMSI,
                       .amfsetid = amf_set_id,
                       .amfpointer = amf_pointer,
                       .tmsi = tmsi},
      .has_pdu_session_status = true,
      .pdu_session_status = {[2] = PDU_SESSION_ACTIVE, [4] = PDU_SESSION_ACTIVE},
      .fgsnasmessagecontainer = NULL};

  original_msg.fgsnasmessagecontainer = calloc_or_fail(1, sizeof(*original_msg.fgsnasmessagecontainer));
  FGCNasMessageContainer *nas_container = original_msg.fgsnasmessagecontainer;
  OctetString *nas_container_contents = &nas_container->nasmessagecontainercontents;
  nas_container_contents->value = calloc_or_fail(sizeof(container_data), sizeof(*nas_container_contents->value));
  memcpy(nas_container_contents->value, container_data, sizeof(container_data));
  nas_container_contents->length = sizeof(container_data);

  uint8_t expected_encoded_data[] = {0x71,
                                     0x00,
                                     0x07,
                                     0xF4,
                                     (amf_set_id >> 2) & 0xFF,
                                     (amf_set_id << 6) | (amf_pointer & 0x3F),
                                     (tmsi >> 24) & 0xFF,
                                     (tmsi >> 16) & 0xFF,
                                     (tmsi >> 8) & 0xFF,
                                     tmsi & 0xFF,
                                     IEI_PDU_SESSION_STATUS,
                                     0x02,
                                     0x14,
                                     0x00,
                                     IEI_FGS_NAS_MESSAGE_CONTAINER,
                                     0x00,
                                     0x03,
                                     0xde,
                                     0xad,
                                     0xbe,
                                     IEI_UE_REQUEST_TYPE,
                                     0x01,
                                     0x00};
  int encoded_service_request_len = sizeof(expected_encoded_data) - 3;

  // Buffer
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Encode NAS Service Request
  int encoded_length = encode_fgs_service_request(buffer, &original_msg, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgs_service_request() failed\n");
  AssertFatal(encoded_length == encoded_service_request_len, "Unexpected encoded SERVICE REQUEST length\n");

  // Compare the raw encoded buffer with expected encoded data, excluding the skipped optional IE.
  AssertFatal(memcmp(buffer, expected_encoded_data, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Request
  fgs_service_request_msg_t decoded_service_request = {0};
  int decoded_length = decode_fgs_service_request(&decoded_service_request, expected_encoded_data, sizeof(expected_encoded_data));
  AssertFatal(decoded_length == sizeof(expected_encoded_data), "decode_fgs_service_request() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_fgs_service_request(&original_msg, &decoded_service_request),
              "test_service_request() failed: original and decoded messages do not match\n");
  free_fgs_service_request(&decoded_service_request);
  free_fgs_service_request(&original_msg);
}

/**
 * @brief Test NAS Service Accept enc/dec
 */
static void test_service_accept(void)
{
  // Dummy NAS Service Accept message
  fgs_service_accept_msg_t orig = {
      .has_psi_status = true,
      .has_psi_res = true,
      .num_errors = 1,
      .t3448 = malloc_or_fail(sizeof(*orig.t3448)),
  };
  orig.psi_status[1] = PDU_SESSION_ACTIVE;
  orig.psi_status[2] = PDU_SESSION_INACTIVE;
  orig.psi_status[3] = PDU_SESSION_ACTIVE;
  orig.psi_res[1] = REACTIVATION_FAILED;
  orig.psi_res[2] = REACTIVATION_SUCCESS;
  orig.cause[0].cause = Illegal_UE;
  orig.cause[0].pdu_session_id = 1;
  orig.t3448->value = 16;
  orig.t3448->unit = TWO_SECONDS;

  // Expected encoded data
  uint8_t expected_enc[] = {0x50, 0x02, 0x0a, 0x00, 0x26, 0x02, 0x02, 0x00, 0x72, 0x00, 0x00, 0x01, 0x03, 0x6B, 0x01, 0x10};
  uint16_t len_status = 2;
  expected_enc[9] = (len_status >> 8) & 0xFF;
  expected_enc[10] = len_status & 0xFF;

  // Buffer
  uint8_t buf[64] = {0};
  byte_array_t buffer = {.buf = &buf[0], .len = sizeof(expected_enc)};

  // Encode NAS Service Accept
  int encoded_length = encode_fgs_service_accept(&buffer, &orig);
  AssertFatal(encoded_length == sizeofArray(expected_enc), "encode_fgs_service_accept() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer.buf, expected_enc, buffer.len) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Accept
  fgs_service_accept_msg_t dec = {0};
  int decoded_length = decode_fgs_service_accept(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgs_service_accept() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_accept(&orig, &dec), "test_service_accept() failed: original and decoded messages do not match\n");
  free_fgs_service_accept(&dec);
  free_fgs_service_accept(&orig);
}

/** @brief Test NAS Service Reject enc/dec */
static void test_service_reject(void)
{
  // Dummy NAS Service Reject message
  fgs_service_reject_msg_t orig = {
      .cause = Illegal_UE,
      .has_psi_status = true,
      .t3446 = malloc_or_fail(sizeof(*orig.t3446)),
      .t3448 = malloc_or_fail(sizeof(*orig.t3448)),
  };
  orig.psi_status[1] = PDU_SESSION_INACTIVE; // PDU Session 1
  orig.psi_status[2] = PDU_SESSION_ACTIVE; // PDU Session 2
  orig.t3446->value = 10;
  orig.t3446->unit = TWO_SECONDS;
  orig.t3448->value = 1;
  orig.t3448->unit = ONE_MINUTE;

  // Expected encoded data
  uint8_t expected_enc[] = {0x03, 0x50, 0x02, 0x04, 0x00, 0x5F, 0x01, 0x0A, 0x6B, 0x01, 0x21};

  // Buffer
  uint8_t buf[64] = {0};
  byte_array_t buffer = {.buf = &buf[0], .len = sizeof(expected_enc)};

  // Encode NAS Service Reject
  int encoded_length = encode_fgs_service_reject(&buffer, &orig);
  AssertFatal(encoded_length == sizeofArray(expected_enc), "encode_fgs_service_reject() failed: %d != %ld\n", encoded_length, sizeofArray(expected_enc));

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer.buf, expected_enc, buffer.len) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Reject
  fgs_service_reject_msg_t dec = {0};
  int decoded_length = decode_fgs_service_reject(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgs_service_reject() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_reject(&orig, &dec), "test_service_reject() failed: original and decoded messages do not match\n");
  free_fgs_service_reject(&dec);
  free_fgs_service_reject(&orig);
}

/** @brief Test NAS Registration Reject enc/dec (plain AMF reject: cause only) */
static void test_registration_reject(void)
{
  fgs_registration_reject_msg_t orig = {
      .cause = Illegal_UE,
  };

  uint8_t expected_enc[] = {0x03};

  uint8_t buf[64] = {0};
  byte_array_t buffer = {.buf = buf, .len = sizeof(expected_enc)};

  int encoded_length = encode_fgs_registration_reject(&buffer, &orig);
  AssertFatal(encoded_length == sizeofArray(expected_enc),
              "encode_fgs_registration_reject() failed: %d != %ld\n",
              encoded_length,
              sizeofArray(expected_enc));
  AssertFatal(memcmp(buffer.buf, expected_enc, buffer.len) == 0, "Encoding mismatch!\n");

  fgs_registration_reject_msg_t dec = {0};
  int decoded_length = decode_fgs_registration_reject(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgs_registration_reject() failed\n");
  AssertFatal(eq_registration_reject(&orig, &dec),
              "test_registration_reject() failed: original and decoded messages do not match\n");
  free_fgs_registration_reject(&dec);
}

/** @brief Test NAS Authentication Failure enc/dec */
static void test_auth_failure(void)
{
  // Dummy NAS Authentication Failure message
  uint8_t afp[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  const fgmm_auth_failure_t orig = {
      .cause = Synch_failure,
      .authentication_failure_param.len = 6,
      .authentication_failure_param.buf = afp,
  };

  // Expected encoded data
  uint8_t expected_enc[] = {0x15, 0x30, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  // Buffer
  uint8_t buf[16] = {0};
  byte_array_t buffer = {.buf = buf, .len = sizeof(buf)};

  // Encode
  int encoded_length = encode_fgmm_auth_failure(&buffer, &orig);
  AssertFatal(encoded_length == sizeofArray(expected_enc), "encode_fgmm_auth_failure() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer.buf, expected_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode
  fgmm_auth_failure_t dec = {0};
  int decoded_length = decode_fgmm_auth_failure(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgmm_auth_failure() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_fgmm_auth_failure(&orig, &dec), "test_auth_failure() failed: original and decoded messages do not match\n");
  free_fgmm_auth_failure(&dec);
}

/** @brief Test NAS Security Mode Reject enc/dec */
static void test_security_mode_reject(void)
{
  // Dummy NAS Security Mode Reject message
  const fgs_security_mode_reject_msg orig = {
      .cause = Illegal_UE,
  };

  // Expected encoded data
  uint8_t expected_enc[] = {0x03};

  // Buffer
  uint8_t buf[8] = {0};
  byte_array_t buffer = {.buf = buf, .len = sizeof(buf)};

  // Encode
  int encoded_length = encode_fgs_security_mode_reject(&buffer, &orig);
  AssertFatal(encoded_length == sizeofArray(expected_enc), "encode_fgs_security_mode_reject() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer.buf, expected_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode
  fgs_security_mode_reject_msg dec = {0};
  int decoded_length = decode_fgs_security_mode_reject(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgs_security_mode_reject() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_sec_mode_reject(&orig, &dec), "test_sec_mode_reject() failed: original and decoded messages do not match\n");
}

/**
 * @brief Test NAS Authentication Reject enc/dec
 */
static void test_auth_reject(void)
{
  // Dummy NAS Authentication Reject message
  uint8_t dummy_eap_msg[] = {0x12, 0x34, 0x56, 0x78, 0x91, 0x01, 0x23}; // Example EAP message
  fgmm_auth_reject_msg_t orig = {
      .eap_msg.len = sizeof(dummy_eap_msg),
  };
  orig.eap_msg.buf = dummy_eap_msg;

  // Expected encoded data
  uint8_t expected_enc[] = {0x78, 0x00, 0x07, 0x12, 0x34, 0x56, 0x78, 0x91, 0x01, 0x23};

  // Buffer
  uint8_t buf[10] = {0};
  byte_array_t buffer = {.buf = buf, .len = sizeof(buf)};

  // Encode
  int encoded_length = encode_fgmm_auth_reject(&buffer, &orig);
  AssertFatal(encoded_length >= 0, "encode_fgmm_auth_reject() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(encoded_length == sizeof(expected_enc), "Encoded length mismatch %d != %ld \n", encoded_length, sizeof(expected_enc));
  AssertFatal(memcmp(buffer.buf, expected_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode
  fgmm_auth_reject_msg_t dec = {0};
  uint8_t eap_msg[1500] = {0};
  dec.eap_msg.buf = eap_msg;
  int decoded_length = decode_fgmm_auth_reject(&dec, &buffer);
  AssertFatal(decoded_length >= 0, "decode_fgmm_auth_reject() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_auth_reject(&orig, &dec), "test_auth_reject() failed: original and decoded messages do not match\n");
}

/**
 * @brief Test NAS PLMN selection (nas_get_selected_plmn): IMSI vs SIB1 PLMN list
 */
static void test_nas_get_selected_plmn(void)
{
  uicc_t uicc = {0};
  nr_ue_nas_t nas = {0};
  nas.uicc = &uicc;
  long sel;
  const long SENTINEL = -999;

  const plmn_id_t p20893_2 = {.mcc = 208, .mnc = 93, .mnc_digit_length = 2};
  const plmn_id_t p20894_2 = {.mcc = 208, .mnc = 94, .mnc_digit_length = 2};
  const plmn_id_t p208093_3 = {.mcc = 208, .mnc = 93, .mnc_digit_length = 3}; /* MNC 093 */
  const plmn_id_t p310260_3 = {.mcc = 310, .mnc = 260, .mnc_digit_length = 3};
  const plmn_id_t zero = {0}; /* placeholder */

  /* 1. 2-digit MNC hit -> index 1 */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  plmn_id_t t1[] = {p20893_2};
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t1, 1, &sel) && sel == 1, "case 1 (2-digit hit)\n");

  /* 2. 3-digit MNC hit -> index 1 */
  uicc.imsiStr = "310260000000000"; uicc.nmc_size = 3;
  plmn_id_t t2[] = {p310260_3};
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t2, 1, &sel) && sel == 1, "case 2 (3-digit hit)\n");

  /* 3. leading-zero MNC 093 (3-digit) -> index 1 */
  uicc.imsiStr = "208093000000000"; uicc.nmc_size = 3;
  plmn_id_t t3[] = {p208093_3};
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t3, 1, &sel) && sel == 1, "case 3 (MNC 093)\n");

  /* 4. same MCC, different MNC -> no match */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  plmn_id_t t4[] = {p20894_2};
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t4, 1, &sel) && sel == SENTINEL, "case 4 (diff MNC)\n");

  /* 5. different MCC, same MNC -> no match */
  plmn_id_t t5[] = {{.mcc = 310, .mnc = 93, .mnc_digit_length = 2}};
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t5, 1, &sel) && sel == SENTINEL, "case 5 (diff MCC)\n");

  /* 6. same MNC value, different mnc_digit_length -> no match (093 vs 93) */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  plmn_id_t t6[] = {p208093_3};
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t6, 1, &sel) && sel == SENTINEL, "case 6 (digit length differs)\n");

  /* 7/8/9. first/second/third entry hit -> 1/2/3 */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  plmn_id_t t789[] = {p20893_2, p20894_2, {.mcc = 208, .mnc = 95, .mnc_digit_length = 2}};
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t789, 3, &sel) && sel == 1, "case 7 (first -> 1)\n");
  uicc.imsiStr = "208940000000000";
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t789, 3, &sel) && sel == 2, "case 8 (second -> 2)\n");
  uicc.imsiStr = "208950000000000";
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t789, 3, &sel) && sel == 3, "case 9 (third -> 3)\n");

  /* 10. middle zero placeholder, later hit keeps original 1-based index (3) */
  uicc.imsiStr = "208950000000000"; uicc.nmc_size = 2;
  plmn_id_t t10[] = {p20893_2, zero, {.mcc = 208, .mnc = 95, .mnc_digit_length = 2}};
  sel = SENTINEL; AssertFatal(nas_get_selected_plmn(&nas, t10, 3, &sel) && sel == 3, "case 10 (placeholder keeps index)\n");

  /* 11. all placeholders -> false */
  plmn_id_t t11[] = {zero, zero, zero};
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t11, 3, &sel) && sel == SENTINEL, "case 11 (all placeholder)\n");

  /* 12/13. no match -> false, output sentinel unchanged */
  uicc.imsiStr = "999990000000000"; uicc.nmc_size = 2;
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t789, 3, &sel) && sel == SENTINEL, "case 12/13 (no match, sentinel kept)\n");

  /* 14. IMSI too short -> false */
  uicc.imsiStr = "208"; uicc.nmc_size = 2;
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t1, 1, &sel) && sel == SENTINEL, "case 14 (IMSI too short)\n");

  /* 15. MNC length not 2 or 3 -> false */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 4;
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t1, 1, &sel) && sel == SENTINEL, "case 15 (bad nmc_size)\n");

  /* 16/17/18. null nas / null plmns / null output -> false */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  AssertFatal(!nas_get_selected_plmn(NULL, t1, 1, &sel), "case 16 (null nas)\n");
  AssertFatal(!nas_get_selected_plmn(&nas, NULL, 1, &sel), "case 17 (null plmns)\n");
  AssertFatal(!nas_get_selected_plmn(&nas, t1, 1, NULL), "case 18 (null out)\n");

  /* 18b/18c. null uicc / null imsiStr -> false, output unchanged */
  nr_ue_nas_t nas_no_uicc = {0};
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas_no_uicc, t1, 1, &sel) && sel == SENTINEL, "case 18b (null uicc)\n");
  uicc_t uicc_no_imsi = {0};
  nr_ue_nas_t nas_no_imsi = {0};
  nas_no_imsi.uicc = &uicc_no_imsi;
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas_no_imsi, t1, 1, &sel) && sel == SENTINEL, "case 18c (null imsiStr)\n");

  /* 19. num_plmns == 0 -> false */
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t1, 0, &sel) && sel == SENTINEL, "case 19 (num_plmns 0)\n");

  /* 20. no scan beyond num_plmns: match sits at index 1 but num_plmns limits to 1 -> false */
  uicc.imsiStr = "208940000000000"; uicc.nmc_size = 2;
  sel = SENTINEL; AssertFatal(!nas_get_selected_plmn(&nas, t789, 1, &sel) && sel == SENTINEL, "case 20 (no over-scan)\n");

  /* 21. capacity (independent of PLMN_LIST_MAX_SIZE value): only the last entry
   * matches -> index max; num_plmns > capacity -> false, output unchanged. */
  plmn_id_t full[PLMN_LIST_MAX_SIZE];
  for (int i = 0; i < PLMN_LIST_MAX_SIZE; i++)
    full[i] = p20894_2; /* all non-matching */
  full[PLMN_LIST_MAX_SIZE - 1] = p20893_2; /* only last matches */
  uicc.imsiStr = "208930000000003"; uicc.nmc_size = 2;
  sel = SENTINEL;
  AssertFatal(nas_get_selected_plmn(&nas, full, PLMN_LIST_MAX_SIZE, &sel) && sel == PLMN_LIST_MAX_SIZE, "case 21a (== capacity)\n");
  sel = SENTINEL;
  AssertFatal(!nas_get_selected_plmn(&nas, full, PLMN_LIST_MAX_SIZE + 1, &sel) && sel == SENTINEL, "case 21b (> capacity)\n");

  LOG_A(NAS, "test_nas_get_selected_plmn() passed\n");
}

int main()
{
  // Initialize logging system
  logInit();
  // Set logging level for NAS to show INFO messages
  set_log(NAS, OAILOG_DEBUG);
  // Tests list
  test_regression_registration_accept();
  test_service_request();
  test_service_accept();
  test_service_reject();
  test_registration_reject();
  test_auth_failure();
  test_auth_reject();
  test_security_mode_reject();
  test_nas_get_selected_plmn();
  return 0;
}
