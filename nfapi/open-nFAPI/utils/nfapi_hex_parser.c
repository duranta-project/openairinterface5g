/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nr_fapi.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p5_utils.h"
#include "nr_fapi_p7.h"
#include "nr_fapi_p7_utils.h"
#define MAX_BUFFER_SIZE (1024 * 1024 * 2)

char *message_id_to_str(uint32_t message_id)
{
  switch (message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      return "NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE";
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      return "NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE";
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_START_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      return "NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE";
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      return "NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST";
    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION";
    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      return "NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION";
    default:
      return "UNKNOWN PHY MSG TYPE";
  }
}

void unpack_and_dump_message(void *buf, fapi_message_header_t hdr)
{
  void *unpacked_req = calloc(1,MAX_BUFFER_SIZE);
  AssertFatal(unpacked_req != NULL, "malloc failed");
  if (hdr.message_id >= NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST && hdr.message_id <= NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION) {
    const int ret = fapi_nr_p5_message_unpack(buf, hdr.message_length + NFAPI_HEADER_LENGTH, unpacked_req, MAX_BUFFER_SIZE, 0);
    DevAssert(ret >= 0);
  } else if (hdr.message_id >= NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST && hdr.message_id <= NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION) {
    const int ret = fapi_nr_p7_message_unpack(buf, hdr.message_length + NFAPI_HEADER_LENGTH, unpacked_req, MAX_BUFFER_SIZE, 0);
    DevAssert(ret >= 0);
  }

  switch (hdr.message_id) {
    /* P5 messages */
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST: {
      dump_param_request(unpacked_req);
      free_param_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE: {
      dump_param_response(unpacked_req);
      free_param_response(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST: {
      dump_config_request(unpacked_req);
      free_config_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE: {
      dump_config_response(unpacked_req);
      free_config_response(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST: {
      dump_start_request(unpacked_req);
      free_start_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE: {
      dump_start_response(unpacked_req);
      free_start_response(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST: {
      dump_stop_request(unpacked_req);
      free_stop_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION: {
      dump_stop_indication(unpacked_req);
      free_stop_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION: {
      dump_error_indication(unpacked_req);
      free_error_indication(unpacked_req);
      return;
    }
    /* P7 messages */
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST: {
      dump_dl_tti_request(unpacked_req);
      free_dl_tti_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST: {
      dump_ul_tti_request(unpacked_req);
      free_ul_tti_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION: {
      dump_slot_indication(unpacked_req);
      free_slot_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST: {
      dump_ul_dci_request(unpacked_req);
      free_ul_dci_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST: {
      dump_tx_data_request(unpacked_req);
      free_tx_data_request(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION: {
      dump_rx_data_indication(unpacked_req);
      free_rx_data_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION: {
      dump_crc_indication(unpacked_req);
      free_crc_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION: {
      dump_uci_indication(unpacked_req);
      free_crc_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION: {
      dump_srs_indication(unpacked_req);
      free_srs_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION: {
      dump_srs_toa_vendor_ext_indication(unpacked_req);
      free_srs_toa_vendor_ext_indication(unpacked_req);
      return;
    }
    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION: {
      dump_rach_indication(unpacked_req);
      free_rach_indication(unpacked_req);
      return;
    }
    default:
      printf("Unknown Msg ID 0x%02x\n", hdr.message_id);
  }
  free(unpacked_req);
}

static uint8_t to_hex(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  printf("found invalid char %c\n", c);
  return 0xff;
}

/* @brief decodes one byte in text representation (0xab or just ab) of s and places it in b
 * @return the number of bytes read, or -1 on error */
static int parse_byte(const char *s, uint8_t *b)
{
  if (!s)
    return -1;
  int c = 0;
  // skip any whitespaces
  while (s[0] && isspace(*s)) {
    c++;
    s++;
  }
  // end of string -> nothing to be read
  if (s[0] == 0)
    return -1;
  // jump over leading "0x"
  if (s[0] == '0' && s[1] == 'x') {
    c += 2;
    s += 2;
  }
  // only one digit, if s[1] has other garbage, it will be caught after
  if (s[1] == 0 || s[1] == ' ') {
    // only one digit
    *b = to_hex(s[0]);
    // check if valid
    return *b != 0xff ? c + 1 : -1;
  }
  // read both bytes, check if valid
  uint8_t hb = to_hex(s[0]);
  uint8_t lb = to_hex(s[1]);
  if (hb == 0xff || lb == 0xff)
    return -1;
  *b = ((hb & 0xf) << 4) | (lb & 0xf);
  return c + 2;
}

int main(int argc, char *argv[])
{
  // Check if there are enough arguments
  if (argc < 2) {
    printf("Usage: %s [0xNN 0xNN ... | NN NN ... ]\n", argv[0]);
    return 1;
  }

  uint8_t hex_values[MAX_BUFFER_SIZE]; // Array to hold the parsed values, assuming max 1024 values
  int byte_count = 0;

  for (int i = 1; i < argc; ++i) {
    const char *l = argv[i];
    int ret;
    do {
      ret = parse_byte(l, &hex_values[byte_count]);
      if (ret > 0)
        byte_count++;
      l += ret;
    } while (ret > 0 && byte_count < MAX_BUFFER_SIZE);
  }

  // Print the converted values
  printf("Parsed %d bytes:\n", byte_count);
  for (int i = 0; i < byte_count; i++) {
    if (i % 16 == 0 && i != 0) {
      printf("\n"); // Start a new line every 16 bytes
    } else if (i % 8 == 0 && i % 16 != 0) {
      printf("  "); // Add extra space between 8-byte sections
    }
    printf("%02x ", hex_values[i]);
  }
  printf("\n");
  AssertFatal(byte_count >= sizeof(fapi_message_header_t), "Read byte amount not enough to encode the header. Should have a minimum of %lu bytes, aborting...", sizeof(fapi_message_header_t));
  uint8_t *ptr = hex_values; // Points to the first element of the array

  fapi_message_header_t hdr = {.num_msg = 0, .opaque_handle = 0, .message_id = 0, .message_length = 0};
  fapi_nr_message_header_unpack(ptr, NFAPI_HEADER_LENGTH, &hdr, sizeof(fapi_message_header_t), 0);

  printf("Decoded message %s message length 0x%02x\n", message_id_to_str(hdr.message_id), hdr.message_length);
  // Unpack the message
  unpack_and_dump_message((uint8_t *)hex_values, hdr);

  return 0;
}
