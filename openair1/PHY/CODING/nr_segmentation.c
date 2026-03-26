/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * \brief Procedures for transport block segmentation for NR (LDPC-coded transport channels)
 */

#include "common/utils/LOG/log.h"
#include "openair1/PHY/CODING/coding_defs.h"
//#define DEBUG_SEGMENTATION

uint32_t get_C(uint32_t B, uint32_t Kcb)
{
  if (B <= Kcb)
    return 1;
  int L = 24;
  uint32_t C = B / (Kcb - L);
  if ((Kcb - L) * C < B)
    C++;
  return C;
}

uint32_t get_K(uint32_t Zout, uint8_t BG)
{
  return BG == 1 ? Zout * 22 : Zout * 10;
}

uint32_t get_Kb(uint8_t BG, uint32_t B)
{
  if (BG == 1)
    return 22;
  if (B > 640) {
    return 10;
  } else if (B > 560) {
   return 9;
  } else if (B > 192) {
    return 8;
  } else {
    return 6;
  }
}

uint32_t get_Zout(uint32_t Kb, uint32_t Kprime)
{
  int Z = (Kprime / Kb) + (Kprime % Kb > 0);
  AssertFatal(Z <= 384, "Illegal codeword size in segmentation\n");
  int Zout = 0;
  if (Z <= 2) {
    Zout = 2;
  } else if (Z <= 16) { // increase by 1 byte til here
    Zout = Z;
  } else if (Z <= 32) { // increase by 2 bytes til here
    Zout = (Z >> 1) << 1;
    if (Zout < Z)
      Zout += 2;
  } else if (Z <= 64) { // increase by 4 bytes til here
    Zout = (Z >> 2) << 2;
    if (Zout < Z)
      Zout += 4;
  } else if (Z <= 128) { // increase by 8 bytes til here
    Zout = (Z >> 3) << 3;
    if (Zout < Z)
      Zout += 8;
  } else if (Z <= 256) { // increase by 4 bytes til here
    Zout = (Z >> 4) << 4;
    if (Zout < Z)
      Zout += 16;
  } else if (Z <= 384) { // increase by 4 bytes til here
    Zout = (Z >> 5) << 5;
    if (Zout < Z)
      Zout += 32;
  }
#ifdef DEBUG_SEGMENTATION
    printf("Z_by_C %u , K2 %u\n", Z, *K);
#endif
  return Zout;
}

int32_t nr_segmentation(unsigned char *input_buffer,
                        unsigned char **output_buffers,
                        unsigned int B,
                        unsigned int *C,
                        unsigned int *K,
                        unsigned int *Zout, // [hna] Zout is Zc
                        unsigned int *F,
                        uint8_t BG)
{

  unsigned int L, Bprime;
  unsigned int Kcb = BG == 1 ? 8448 : 3840;
  *C = get_C(B, Kcb);
  if (B <= Kcb) {
    L = 0;
    Bprime = B;
  } else {
    L = 24;
    Bprime = B + ((*C) * L);
#ifdef DEBUG_SEGMENTATION
    printf("Bprime %u\n", Bprime);
#endif
  }

  // Find K+
  unsigned int Kprime = Bprime / (*C);
  LOG_D(PHY,"nr segmentation B %u Bprime %u Kprime %u\n", B, Bprime, Kprime);
  unsigned int Kb = get_Kb(BG, B);  
  *Zout = get_Zout(Kb, Kprime);
  *K = get_K(*Zout, BG);
  *F = ((*K) - Kprime);

  LOG_D(PHY,"final nr seg output Z %u K %u F %u \n", *Zout, *K, *F);
  LOG_D(PHY,"C %u, K %u, Bprime_bytes %u, Bprime %u, F %u\n",*C, *K, Bprime>>3, Bprime, *F);

  if ((input_buffer) && (output_buffers)) {
    int s = 0;
    for (int r = 0; r < *C; r++) {
      memcpy(output_buffers[r],input_buffer + s, (Kprime - L) >> 3);
      s += (Kprime -L ) >> 3;
      if (*C > 1) { // add CRC
        unsigned int crc = crc24b(output_buffers[r], Kprime - L) >> 8;
        output_buffers[r][(Kprime - L) >> 3] = ((uint8_t*)&crc)[2];
        output_buffers[r][1+((Kprime - L) >> 3)] = ((uint8_t*)&crc)[1];
        output_buffers[r][2+((Kprime - L) >> 3)] = ((uint8_t*)&crc)[0];
      }
      if (*F > 0) {
        for (int k = Kprime >> 3; k < (*K >> 3); k++) {
          output_buffers[r][k] = 0;
          //printf("r %d filler bits [%d] = %d Kprime %d \n", r,k, output_buffers[r][k], Kprime);
        }
      }
    }
  }
  return Kb;
}

#ifdef MAIN
main()
{
  unsigned int K, C, F, Bbytes, Zout;
  for (Bbytes =5 ; Bbytes < 8; Bbytes++) {
    nr_segmentation(0, 0, Bbytes << 3, &C, &K, &Zout, &F);
    printf("Bbytes %u : C %u, K %u, F %u\n", Bbytes, C, K, F);
  }
}
#endif
