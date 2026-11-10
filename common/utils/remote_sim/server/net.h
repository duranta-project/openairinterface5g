/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NET_H_
#define _NET_H_

#include <stdbool.h>

typedef enum {
  NET_AUTN,         /* sent by client to request an AUTHENTICATE */
  NET_RES,          /* sent by server when AUTHENTICATE succeeds */
  NET_AUTS,         /* sent by server when AUTHENTICATE sync fails */
  NET_ERR,          /* sent by server when AUTHENTICATE fails (for whatever reason) */
  NET_GET_IMSI,     /* sent by client to get IMSI */
  NET_IMSI,         /* sent by server */
  NET_CARD_READY    /* sent by server after opening simcard */
} net_message_type_t;

typedef struct {
  net_message_type_t type;
  union {
    struct {
      unsigned char rand[16];
      unsigned char autn[16];
    } autn;
    struct {
      unsigned char res_len;
      unsigned char res[16];
      unsigned char ck[16];
      unsigned char ik[16];
    } res;
    struct {
      unsigned char auts[14];
    } auts;
    struct {
      /* only IMSI of 15 digits supported, 'imsi' is NULL-Z ASCII */
      char imsi[16];
    } imsi;
  };
} net_message_t;

int net_get_client(char *listen_ip, int listen_port);
void net_close(int s);
bool net_send(int s, net_message_t *m);
bool net_receive(int s, net_message_t *m);

#endif /* _NET_H_ */
