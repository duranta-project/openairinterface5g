/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Top-level routines for implementing LDPC-coded (DLSCH) transport channels from 38-212, 15.2
 */

#include "nr_transport_common_proto.h"
#include "PHY/CODING/coding_defs.h"
#include "PHY/defs_nr_common.h"
#include "log.h"

uint32_t
nr_get_G(uint16_t nb_rb, uint16_t nb_symb_sch, uint8_t nb_re_dmrs, uint16_t length_dmrs, uint32_t unav_res, uint8_t Qm, uint8_t Nl)
{
  uint32_t G = ((NR_NB_SC_PER_RB * nb_symb_sch) - (nb_re_dmrs * length_dmrs)) * nb_rb * Qm * Nl;
  G -= unav_res * Qm * Nl;
  return (G);
}

uint32_t nr_get_G_SL(uint16_t nb_rb, uint16_t nb_symb_sch, uint8_t nb_re_dmrs, uint16_t length_dmrs, uint8_t sci1_dmrs_overlap, uint16_t sci1_re, uint16_t sci1_rb, uint16_t sci2_re, uint16_t csi_rs_re, uint8_t Qm, uint8_t Nl) {

  uint32_t G_SL, slsch_re;
  slsch_re = ((NR_NB_SC_PER_RB*nb_symb_sch) - (nb_re_dmrs*length_dmrs)) * nb_rb;
  if (sci1_dmrs_overlap > 0) slsch_re += (nb_re_dmrs * sci1_rb); // return the dmrs that are not transmitted due to SCI1
  slsch_re -= sci1_re; // REs taken by SCI1
  slsch_re -= sci2_re; // REs taken by SCI2
  slsch_re -= csi_rs_re; // REs taken by CSI_RS
  G_SL = slsch_re * Qm * Nl;
  return(G_SL);
}

int nr_get_E(uint32_t G, uint8_t C, uint8_t Qm, uint8_t Nl, uint8_t r)
{
  if (Nl <= 0 || Qm <= 0) {
    LOG_E(PHY, "Invalid input parameter in nr_get_E: Qm %d Nl %d\n", Qm, Nl);
    return -1;
  }

  uint32_t E;
  uint8_t Cprime = C; // assume CBGTI not present
  if (r <= Cprime - ((G / (Nl * Qm)) % Cprime) - 1)
    E = Nl * Qm * (G / (Nl * Qm * Cprime));
  else
    E = Nl * Qm * ((G / (Nl * Qm * Cprime)) + 1);

  LOG_D(PHY, "nr_get_E : (G %d, C %d, Qm %d, Nl %d, r %d), E %d\n", G, C, Qm, Nl, r, E);
  return E;
}

static const uint8_t index_k0[2][4] = {{0, 17, 33, 56}, {0, 13, 25, 43}};

int nr_get_R_ldpc_decoder(int rvidx, int E, int BG, int Z, int *llrLen, int round)
{
  AssertFatal(BG == 1 || BG == 2, "Unknown BG %d\n", BG);

  int Ncb = (BG == 1) ? (66 * Z) : (50 * Z);
  int infoBits = (index_k0[BG - 1][rvidx] * Z + E);

  if (round == 0)
    *llrLen = infoBits;
  if (infoBits > Ncb)
    infoBits = Ncb;
  if (infoBits > *llrLen)
    *llrLen = infoBits;

  int sysBits = (BG == 1) ? (22 * Z) : (10 * Z);
  float decoderR = (float)sysBits / (infoBits + 2 * Z);

  if (BG == 2)
    if (decoderR < 0.3333)
      return 15;
    else if (decoderR < 0.6667)
      return 13;
    else
      return 23;
  else if (decoderR < 0.6667)
    return 13;
  else if (decoderR < 0.8889)
    return 23;
  else
    return 89;
}
