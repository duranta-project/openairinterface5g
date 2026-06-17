/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef _NRLDPC_EXTERN_H__
#define _NRLDPC_EXTERN_H__
#include "openair1/PHY/CODING/nrLDPC_defs.h"
/* ldpc coder/decoder API*/
typedef struct ldpc_interface_s {
  LDPC_initfunc_t *LDPCinit;
  LDPC_shutdownfunc_t *LDPCshutdown;
  LDPC_decoderfunc_t *LDPCdecoder;
  LDPC_encoderfunc_t *LDPCencoder;
  LDPC_encoderfunc32_t *LDPCencoder32; /* only for LDPC CUDA, don't use */
} ldpc_interface_t;

/* functions to load the LDPC shared lib, implemented in openair1/PHY/CODING/nrLDPC_load.c */
int load_LDPClib(char *version, ldpc_interface_t *);
int free_LDPClib(ldpc_interface_t *ldpc_interface);

LDPC_decoderfunc_t LDPCdecoder;
LDPC_encoderfunc_t LDPCencoder;
LDPC_encoderfunc32_t LDPCencoder32;

// inline functions:
#endif
