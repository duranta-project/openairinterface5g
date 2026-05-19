/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Return values:
 *  0 --> Success
 * -1 --> All list entries have failed the CRC checks
 */

#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "assertions.h"

// #define POLAR_CODING_DEBUG

static inline void updateCrcChecksum2(int xlen,
                                      int ylen,
                                      uint8_t crcChecksum[xlen][ylen],
                                      int gxlen,
                                      int gylen,
                                      uint8_t crcGen[gxlen][gylen],
                                      uint8_t listSize,
                                      uint32_t i2,
                                      uint8_t len)
{
  for (uint i = 0; i < listSize; i++) {
    for (uint j = 0; j < len; j++) {
      crcChecksum[j][i + listSize] = (crcChecksum[j][i] + crcGen[i2][j]) % 2;
    }
  }
}

int8_t polar_decoder(double *input,
                     uint32_t *out,
                     uint8_t listSize,
                     int8_t messageType,
                     uint16_t messageLength,
                     uint8_t aggregation_level)
{
  t_nrPolar_params *polarParams = nr_polar_params(messageType, messageLength, aggregation_level);
  // Assumes no a priori knowledge.
  uint8_t bit[polarParams->N][polarParams->n + 1][2 * listSize];
  memset(bit, 0, sizeof bit);
  uint8_t bitUpdated[polarParams->N][polarParams->n + 1]; // 0=False, 1=True
  memset(bitUpdated, 0, sizeof bitUpdated);
  uint8_t llrUpdated[polarParams->N][polarParams->n + 1]; // 0=False, 1=True
  memset(llrUpdated, 0, sizeof llrUpdated);
  double llr[polarParams->N][polarParams->n + 1][2 * listSize];
  uint8_t crcChecksum[polarParams->crcParityBits][2 * listSize];
  memset(crcChecksum, 0, sizeof crcChecksum);
  double pathMetric[2 * listSize];
  uint8_t crcState[2 * listSize]; // 0=False, 1=True

  for (int i = 0; i < (2 * listSize); i++) {
    pathMetric[i] = 0;
    crcState[i] = 1;
  }

  for (int i = 0; i < polarParams->N; i++) {
    llrUpdated[i][polarParams->n] = 1;
    bitUpdated[i][0] = (polarParams->information_bit_pattern[i] + 1) % 2;
  }

  uint8_t extended_crc_generator_matrix[polarParams->K][polarParams->crcParityBits]; // G_P3
  uint8_t tempECGM[polarParams->K][polarParams->crcParityBits]; // G_P2

  for (int i = 0; i < polarParams->payloadBits; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      tempECGM[i][j] = polarParams->crc_generator_matrix[i][j];
    }
  }

  for (int i = polarParams->payloadBits; i < polarParams->K; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      tempECGM[i][j] = (i - polarParams->payloadBits) == j;
    }
  }

  for (int i = 0; i < polarParams->K; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      extended_crc_generator_matrix[i][j] = tempECGM[polarParams->interleaving_pattern[i]][j];
    }
  }

  // The index of the last 1-valued bit that appears in each column.
  AssertFatal(polarParams->crcParityBits > 0, "UB for VLA. crcParityBits negative\n");
  uint16_t last1ind[polarParams->crcParityBits];

  for (int j = 0; j < polarParams->crcParityBits; j++) {
    for (int i = 0; i < polarParams->K; i++) {
      if (extended_crc_generator_matrix[i][j] == 1)
        last1ind[j] = i;
    }
  }

  double d_tilde[polarParams->N];
  nr_polar_rate_matching(input,
                         d_tilde,
                         polarParams->rate_matching_pattern,
                         polarParams->K,
                         polarParams->N,
                         polarParams->encoderLength);

  for (int j = 0; j < polarParams->N; j++)
    llr[j][polarParams->n][0] = d_tilde[j];

  /*
   * SCL polar decoder.
   */
  uint32_t nonFrozenBit = 0;
  uint currentListSize = 1;
  uint decoderIterationCheck = 0;
  int checkCrcBits = -1;
  uint8_t listIndex[2 * listSize], copyIndex;

  for (uint currentBit = 0; currentBit < polarParams->N; currentBit++) {
    updateLLR(currentListSize, currentBit, 0, polarParams->N, polarParams->n + 1, 2 * listSize, llr, llrUpdated, bit, bitUpdated);

    if (polarParams->information_bit_pattern[currentBit] == 0) { // Frozen bit.
      updatePathMetric(pathMetric, currentListSize, 0, currentBit, polarParams->N, polarParams->n + 1, 2 * listSize, llr);
    } else { // Information or CRC bit.
      updatePathMetric2(pathMetric, currentListSize, currentBit, polarParams->N, polarParams->n + 1, 2 * listSize, llr);

      for (int i = 0; i < currentListSize; i++) {
        for (int j = 0; j < polarParams->N; j++) {
          for (int k = 0; k < (polarParams->n + 1); k++) {
            bit[j][k][i + currentListSize] = bit[j][k][i];
            llr[j][k][i + currentListSize] = llr[j][k][i];
          }
        }
      }

      for (int i = 0; i < currentListSize; i++) {
        bit[currentBit][0][i] = 0;
        crcState[i + currentListSize] = crcState[i];
      }

      for (int i = currentListSize; i < 2 * currentListSize; i++)
        bit[currentBit][0][i] = 1;

      bitUpdated[currentBit][0] = 1;
      updateCrcChecksum2(polarParams->crcParityBits,
                         2 * listSize,
                         crcChecksum,
                         polarParams->K,
                         polarParams->crcParityBits,
                         extended_crc_generator_matrix,
                         currentListSize,
                         nonFrozenBit,
                         polarParams->crcParityBits);
      currentListSize *= 2;

      // Keep only the best "listSize" number of entries.
      if (currentListSize > listSize) {
        for (uint i = 0; i < 2 * listSize; i++)
          listIndex[i] = i;

        nr_sort_asc_double_1D_array_ind(pathMetric, listIndex, currentListSize);
        // sort listIndex[listSize, ..., 2*listSize-1] in descending order.

        for (uint i = 0; i < listSize; i++) {
          int swaps = 0;

          for (uint j = listSize; j < (2 * listSize - i) - 1; j++) {
            if (listIndex[j + 1] > listIndex[j]) {
              int tempInd = listIndex[j];
              listIndex[j] = listIndex[j + 1];
              listIndex[j + 1] = tempInd;
              swaps++;
            }
          }

          if (swaps == 0)
            break;
        }

        // First, backup the best "listSize" number of entries.
        for (int k = (listSize - 1); k > 0; k--) {
          for (int i = 0; i < polarParams->N; i++) {
            for (int j = 0; j < (polarParams->n + 1); j++) {
              bit[i][j][listIndex[(2 * listSize - 1) - k]] = bit[i][j][listIndex[k]];
              llr[i][j][listIndex[(2 * listSize - 1) - k]] = llr[i][j][listIndex[k]];
            }
          }
        }

        for (int k = (listSize - 1); k > 0; k--) {
          for (int i = 0; i < polarParams->crcParityBits; i++) {
            crcChecksum[i][listIndex[(2 * listSize - 1) - k]] = crcChecksum[i][listIndex[k]];
          }
        }

        for (int k = (listSize - 1); k > 0; k--)
          crcState[listIndex[(2 * listSize - 1) - k]] = crcState[listIndex[k]];

        // Copy the best "listSize" number of entries to the first indices.
        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          for (int i = 0; i < polarParams->N; i++) {
            for (int j = 0; j < (polarParams->n + 1); j++) {
              bit[i][j][k] = bit[i][j][copyIndex];
              llr[i][j][k] = llr[i][j][copyIndex];
            }
          }
        }

        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          for (int i = 0; i < polarParams->crcParityBits; i++) {
            crcChecksum[i][k] = crcChecksum[i][copyIndex];
          }
        }

        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          crcState[k] = crcState[copyIndex];
        }

        currentListSize = listSize;
      }

      for (int i = 0; i < polarParams->crcParityBits; i++) {
        if (last1ind[i] == nonFrozenBit) {
          checkCrcBits = i;
          break;
        }
      }

      if (checkCrcBits > (-1)) {
        for (uint i = 0; i < currentListSize; i++) {
          if (crcChecksum[checkCrcBits][i] == 1) {
            crcState[i] = 0; // 0=False, 1=True
          }
        }
      }

      for (uint i = 0; i < currentListSize; i++)
        decoderIterationCheck += crcState[i];

      if (decoderIterationCheck == 0) {
        // perror("[SCL polar decoder] All list entries have failed the CRC checks.");
        polarReturn(polarParams);
        return -1;
      }

      nonFrozenBit++;
      decoderIterationCheck = 0;
      checkCrcBits = -1;
    }
  }

  for (uint i = 0; i < 2 * listSize; i++)
    listIndex[i] = i;

  nr_sort_asc_double_1D_array_ind(pathMetric, listIndex, currentListSize);
  uint8_t nr_polar_A[polarParams->payloadBits];
  for (uint i = 0; i < fmin(listSize, (pow(2, polarParams->crcCorrectionBits))); i++) {
    if (crcState[listIndex[i]] == 1) {
      uint8_t nr_polar_U[polarParams->N];
      for (int j = 0; j < polarParams->N; j++)
        nr_polar_U[j] = bit[j][0][listIndex[i]];

      // Extract the information bits (û to ĉ)
      uint8_t nr_polar_CPrime[polarParams->N];
      nr_polar_info_bit_extraction(nr_polar_U, nr_polar_CPrime, polarParams->information_bit_pattern, polarParams->N);
      // Deinterleaving (ĉ to b)
      uint8_t nr_polar_B[polarParams->K];
      nr_polar_deinterleaver(nr_polar_CPrime, nr_polar_B, polarParams->interleaving_pattern, polarParams->K);

      // Remove the CRC (â)
      memcpy(nr_polar_A, nr_polar_B, polarParams->payloadBits);

      break;
    }
  }

  /*
   * Return bits.
   */
  nr_byte2bit_uint8_32(nr_polar_A, polarParams->payloadBits, out);

  polarReturn(polarParams);
  return 0;
}

int8_t polar_decoder_dci(double *input,
                         uint32_t *out,
                         uint8_t listSize,
                         uint16_t n_RNTI,
                         int8_t messageType,
                         uint16_t messageLength,
                         uint8_t aggregation_level)
{
  t_nrPolar_params *polarParams = nr_polar_params(messageType, messageLength, aggregation_level);

  uint8_t bit[polarParams->N][polarParams->n + 1][2 * listSize];
  memset(bit, 0, sizeof bit);
  uint8_t bitUpdated[polarParams->N][polarParams->n + 1]; // 0=False, 1=True
  memset(bitUpdated, 0, sizeof bitUpdated);
  uint8_t llrUpdated[polarParams->N][polarParams->n + 1]; // 0=False, 1=True
  memset(llrUpdated, 0, sizeof llrUpdated);
  double llr[polarParams->N][polarParams->n + 1][2 * listSize];
  uint8_t crcChecksum[polarParams->crcParityBits][2 * listSize];
  memset(crcChecksum, 0, sizeof crcChecksum);
  double pathMetric[2 * listSize];
  uint8_t crcState[2 * listSize]; // 0=False, 1=True
  uint8_t extended_crc_scrambling_pattern[polarParams->crcParityBits];

  for (int i = 0; i < (2 * listSize); i++) {
    pathMetric[i] = 0;
    crcState[i] = 1;
  }

  for (int i = 0; i < polarParams->N; i++) {
    llrUpdated[i][polarParams->n] = 1;
    bitUpdated[i][0] = (polarParams->information_bit_pattern[i] + 1) % 2;
  }

  uint8_t extended_crc_generator_matrix[polarParams->K][polarParams->crcParityBits]; // G_P3: K-by-P
  uint8_t tempECGM[polarParams->K][polarParams->crcParityBits]; // G_P2: K-by-P

  for (int i = 0; i < polarParams->payloadBits; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      tempECGM[i][j] = polarParams->crc_generator_matrix[i + polarParams->crcParityBits][j];
    }
  }

  for (int i = polarParams->payloadBits; i < polarParams->K; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      tempECGM[i][j] = (i - polarParams->payloadBits) == j;
    }
  }

  for (int i = 0; i < polarParams->K; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++) {
      extended_crc_generator_matrix[i][j] = tempECGM[polarParams->interleaving_pattern[i]][j];
    }
  }

  // The index of the last 1-valued bit that appears in each column.
  uint16_t last1ind[polarParams->crcParityBits];

  for (int j = 0; j < polarParams->crcParityBits; j++) {
    for (int i = 0; i < polarParams->K; i++) {
      if (extended_crc_generator_matrix[i][j] == 1)
        last1ind[j] = i;
    }
  }

  for (int i = 0; i < 8; i++)
    extended_crc_scrambling_pattern[i] = 0;

  for (int i = 8; i < polarParams->crcParityBits; i++) {
    extended_crc_scrambling_pattern[i] = (n_RNTI >> (23 - i)) & 1;
  }

  double d_tilde[polarParams->N];
  nr_polar_rate_matching(input,
                         d_tilde,
                         polarParams->rate_matching_pattern,
                         polarParams->K,
                         polarParams->N,
                         polarParams->encoderLength);

  for (int j = 0; j < polarParams->N; j++)
    llr[j][polarParams->n][0] = d_tilde[j];

  /*
   * SCL polar decoder.
   */

  for (int i = 0; i < polarParams->crcParityBits; i++) {
    for (int j = 0; j < polarParams->crcParityBits; j++)
      crcChecksum[i][0] = crcChecksum[i][0] + polarParams->crc_generator_matrix[j][i];

    crcChecksum[i][0] = (crcChecksum[i][0] % 2);
  }

  uint32_t nonFrozenBit = 0;
  uint currentListSize = 1;
  uint decoderIterationCheck = 0;
  int checkCrcBits = -1;
  uint8_t listIndex[2 * listSize], copyIndex;

  for (uint currentBit = 0; currentBit < polarParams->N; currentBit++) {
    updateLLR(currentListSize, currentBit, 0, polarParams->N, polarParams->n + 1, 2 * listSize, llr, llrUpdated, bit, bitUpdated);

    if (polarParams->information_bit_pattern[currentBit] == 0) { // Frozen bit.
      updatePathMetric(pathMetric, currentListSize, 0, currentBit, polarParams->N, polarParams->n + 1, 2 * listSize, llr);
    } else { // Information or CRC bit.
      updatePathMetric2(pathMetric, currentListSize, currentBit, polarParams->N, polarParams->n + 1, 2 * listSize, llr);

      for (int i = 0; i < currentListSize; i++) {
        for (int j = 0; j < polarParams->N; j++) {
          for (int k = 0; k < (polarParams->n + 1); k++) {
            bit[j][k][i + currentListSize] = bit[j][k][i];
            llr[j][k][i + currentListSize] = llr[j][k][i];
          }
        }
      }

      for (int i = 0; i < currentListSize; i++) {
        bit[currentBit][0][i] = 0;
        crcState[i + currentListSize] = crcState[i];
      }

      for (int i = currentListSize; i < 2 * currentListSize; i++)
        bit[currentBit][0][i] = 1;

      bitUpdated[currentBit][0] = 1;
      updateCrcChecksum2(polarParams->crcParityBits,
                         2 * listSize,
                         crcChecksum,
                         polarParams->K,
                         polarParams->crcParityBits,
                         extended_crc_generator_matrix,
                         currentListSize,
                         nonFrozenBit,
                         polarParams->crcParityBits);
      currentListSize *= 2;

      // Keep only the best "listSize" number of entries.
      if (currentListSize > listSize) {
        for (uint i = 0; i < 2 * listSize; i++)
          listIndex[i] = i;

        nr_sort_asc_double_1D_array_ind(pathMetric, listIndex, currentListSize);
        // sort listIndex[listSize, ..., 2*listSize-1] in descending order.

        for (uint i = 0; i < listSize; i++) {
          int swaps = 0;

          for (uint j = listSize; j < (2 * listSize - i) - 1; j++) {
            if (listIndex[j + 1] > listIndex[j]) {
              int tempInd = listIndex[j];
              listIndex[j] = listIndex[j + 1];
              listIndex[j + 1] = tempInd;
              swaps++;
            }
          }

          if (swaps == 0)
            break;
        }

        // First, backup the best "listSize" number of entries.
        for (int k = (listSize - 1); k > 0; k--) {
          for (int i = 0; i < polarParams->N; i++) {
            for (int j = 0; j < (polarParams->n + 1); j++) {
              bit[i][j][listIndex[(2 * listSize - 1) - k]] = bit[i][j][listIndex[k]];
              llr[i][j][listIndex[(2 * listSize - 1) - k]] = llr[i][j][listIndex[k]];
            }
          }
        }

        for (int k = (listSize - 1); k > 0; k--) {
          for (int i = 0; i < polarParams->crcParityBits; i++) {
            crcChecksum[i][listIndex[(2 * listSize - 1) - k]] = crcChecksum[i][listIndex[k]];
          }
        }

        for (int k = (listSize - 1); k > 0; k--)
          crcState[listIndex[(2 * listSize - 1) - k]] = crcState[listIndex[k]];

        // Copy the best "listSize" number of entries to the first indices.
        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          for (int i = 0; i < polarParams->N; i++) {
            for (int j = 0; j < (polarParams->n + 1); j++) {
              bit[i][j][k] = bit[i][j][copyIndex];
              llr[i][j][k] = llr[i][j][copyIndex];
            }
          }
        }

        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          for (int i = 0; i < polarParams->crcParityBits; i++) {
            crcChecksum[i][k] = crcChecksum[i][copyIndex];
          }
        }

        for (int k = 0; k < listSize; k++) {
          if (k > listIndex[k]) {
            copyIndex = listIndex[(2 * listSize - 1) - k];
          } else { // Use the backup.
            copyIndex = listIndex[k];
          }

          crcState[k] = crcState[copyIndex];
        }

        currentListSize = listSize;
      }

      for (int i = 0; i < polarParams->crcParityBits; i++) {
        if (last1ind[i] == nonFrozenBit) {
          checkCrcBits = i;
          break;
        }
      }

      if (checkCrcBits > (-1)) {
        for (uint i = 0; i < currentListSize; i++) {
          if (crcChecksum[checkCrcBits][i] != extended_crc_scrambling_pattern[checkCrcBits]) {
            crcState[i] = 0; // 0=False, 1=True
          }
        }
      }

      for (uint i = 0; i < currentListSize; i++)
        decoderIterationCheck += crcState[i];

      if (decoderIterationCheck == 0) {
        // perror("[SCL polar decoder] All list entries have failed the CRC checks.");
        polarReturn(polarParams);
        return -1;
      }

      nonFrozenBit++;
      decoderIterationCheck = 0;
      checkCrcBits = -1;
    }
  }

  for (uint i = 0; i < 2 * listSize; i++)
    listIndex[i] = i;

  nr_sort_asc_double_1D_array_ind(pathMetric, listIndex, currentListSize);
  uint8_t nr_polar_A[polarParams->payloadBits];

  for (uint i = 0; i < fmin(listSize, (pow(2, polarParams->crcCorrectionBits))); i++) {
    if (crcState[listIndex[i]] == 1) {
      uint8_t nr_polar_U[polarParams->N];
      for (int j = 0; j < polarParams->N; j++)
        nr_polar_U[j] = bit[j][0][listIndex[i]];

      // Extract the information bits (û to ĉ)
      uint8_t nr_polar_CPrime[polarParams->N];
      nr_polar_info_bit_extraction(nr_polar_U, nr_polar_CPrime, polarParams->information_bit_pattern, polarParams->N);
      // Deinterleaving (ĉ to b)
      uint8_t nr_polar_B[polarParams->K];
      nr_polar_deinterleaver(nr_polar_CPrime, nr_polar_B, polarParams->interleaving_pattern, polarParams->K);

      // Remove the CRC (â)
      memcpy(nr_polar_A, nr_polar_B, polarParams->payloadBits);
      break;
    }
  }

  /*
   * Return bits.
   */
  nr_byte2bit_uint8_32(nr_polar_A, polarParams->payloadBits, out);

  polarReturn(polarParams);
  return 0;
}

static inline void nr_polar_rate_matching_int16(int16_t *input,
                                                int16_t *output,
                                                const uint16_t *rmp,
                                                const uint16_t K,
                                                const uint16_t N,
                                                const uint16_t E,
                                                const uint8_t i_bil)
{
  if (E >= N) { // repetition
    memset(output, 0, N * sizeof(*output));
    for (int i = 0; i <= E - 1; i++)
      output[rmp[i]] += input[i];
  } else {
    if ((K / (double)E) <= (7.0 / 16))
      memset(output, 0, N * sizeof(*output)); // puncturing
    else { // shortening
      for (int i = 0; i <= N - 1; i++)
        output[i] = INT16_MAX;
    }

    for (int i = 0; i <= E - 1; i++)
      output[rmp[i]] = input[i];
  }
}

static inline void nr_polar_info_extraction_from_u(uint64_t *Cprime,
                                                   const uint8_t *u,
                                                   const uint8_t *information_bit_pattern,
                                                   const uint8_t *parity_check_bit_pattern,
                                                   const uint16_t *interleaving_pattern,
                                                   uint16_t N,
                                                   uint8_t n_pc,
                                                   int K)
{
  int k = 0;

  if (n_pc > 0) {
    for (int n = 0; n < N; n++) {
      if (information_bit_pattern[n] == 1 && parity_check_bit_pattern[n] == 0) {
        int targetBit = K - 1 - interleaving_pattern[k];
        int k1 = targetBit >> 6;
        int k2 = targetBit & 63;
        Cprime[k1] |= (uint64_t)u[n] << k2;
        k++;
      }
    }
  } else {
    for (int n = 0; n < N; n++) {
      if (information_bit_pattern[n] == 1) {
        int targetBit = K - 1 - interleaving_pattern[k];
        int k1 = targetBit >> 6;
        int k2 = targetBit & 63;
        Cprime[k1] |= (uint64_t)u[n] << k2;
        k++;
      }
    }
  }
}

uint32_t polar_decoder_int16(int16_t *input,
                             uint64_t *out,
			     uint16_t *nid,
                             uint8_t ones_flag,
                             int8_t messageType,
                             uint16_t messageLength,
                             uint8_t aggregation_level)
{
  t_nrPolar_params *polarParams = nr_polar_params(messageType, messageLength, aggregation_level);
  const uint N = polarParams->N;
#ifdef POLAR_CODING_DEBUG
  printf("\nRX\n");
  printf("rm:");
  for (int i = 0; i < N; i++) {
    if (i % 4 == 0) {
      printf(" ");
    }
    printf("%i", input[i] < 0 ? 1 : 0);
  }
  printf("\n");
#endif

  int16_t d_tilde[N];
  const uint E = polarParams->encoderLength;
  int16_t inbis[E];
  int16_t *input_deinterleaved;
  if (polarParams->i_bil) {
    for (int i = 0; i < E; i++)
      inbis[i] = input[polarParams->i_bil_pattern[i]];
    input_deinterleaved = inbis;
  } else {
    input_deinterleaved = input;
  }

  nr_polar_rate_matching_int16(input_deinterleaved,
                               d_tilde,
                               polarParams->rate_matching_pattern,
                               polarParams->K,
                               N,
                               E,
                               polarParams->i_bil);
#ifdef POLAR_CODING_DEBUG
  printf("d: ");
  for (int i = 0; i < N; i++) {
    if (i % 4 == 0) {
      printf(" ");
    }
    printf("%i", d_tilde[i] < 0 ? 1 : 0);
  }
  printf("\n");
#endif
  uint8_t nr_polar_U[N];
  memset(nr_polar_U, 0, sizeof(nr_polar_U));
  memcpy(treeAlpha(polarParams->decoder.root), d_tilde, sizeof(d_tilde));
  generic_polar_decoder(polarParams, polarParams->decoder.root, nr_polar_U);
#ifdef POLAR_CODING_DEBUG
  printf("u: ");
  for (int i = 0; i < N; i++) {
    if (i % 4 == 0) {
      printf(" ");
    }
    printf("%i", nr_polar_U[i]);
  }
  printf("\n");
#endif

  // Extract the information bits (û to ĉ)
  uint64_t B[4] = {0};
  nr_polar_info_extraction_from_u(B,
                                  nr_polar_U,
                                  polarParams->information_bit_pattern,
                                  polarParams->parity_check_bit_pattern,
                                  polarParams->interleaving_pattern,
                                  N,
                                  polarParams->n_pc,
                                  polarParams->K);

#ifdef POLAR_CODING_DEBUG
  printf("c: ");
  for (int n = 0; n < polarParams->K; n++) {
    if (n % 4 == 0) {
      printf(" ");
    }
    int n1 = n >> 6;
    int n2 = n - (n1 << 6);
    printf("%lu", (B[n1] >> n2) & 1);
  }
  printf("\n");
#endif

  int len = polarParams->payloadBits;
  // int len_mod64=len&63;
  int crclen = polarParams->crcParityBits;
  uint64_t rxcrc = B[0] & ((1 << crclen) - 1);
  uint32_t crc = 0;
  uint64_t Ar = 0;
  AssertFatal(len < 65, "A must be less than 65 bits\n");

  // appending 24 ones before a0 for DCI as stated in 38.212 7.3.2
  uint8_t offset = 0;
  if (ones_flag)
    offset = 3;

  if (len <= 32) {
    Ar = (uint32_t)(B[0] >> crclen);
    uint8_t A32_flip[4 + offset];
    if (ones_flag) {
      A32_flip[0] = 0xff;
      A32_flip[1] = 0xff;
      A32_flip[2] = 0xff;
    }
    uint32_t Aprime = (uint32_t)(Ar << (32 - len));
    A32_flip[0 + offset] = ((uint8_t *)&Aprime)[3];
    A32_flip[1 + offset] = ((uint8_t *)&Aprime)[2];
    A32_flip[2 + offset] = ((uint8_t *)&Aprime)[1];
    A32_flip[3 + offset] = ((uint8_t *)&Aprime)[0];
    if (crclen == 24)
      crc = (uint64_t)((crc24c(A32_flip, 8 * offset + len) >> 8) & 0xffffff);
    else if (crclen == 11)
      crc = (uint64_t)((crc11(A32_flip, 8 * offset + len) >> 21) & 0x7ff);
    else if (crclen == 6)
      crc = (uint64_t)((crc6(A32_flip, 8 * offset + len) >> 26) & 0x3f);
  } else if (len <= 64) {
    Ar = (B[0] >> crclen) | (B[1] << (64 - crclen));
    uint8_t A64_flip[8 + offset];
    if (ones_flag) {
      A64_flip[0] = 0xff;
      A64_flip[1] = 0xff;
      A64_flip[2] = 0xff;
    }
    uint64_t Aprime = (uint64_t)(Ar << (64 - len));
    A64_flip[0 + offset] = ((uint8_t *)&Aprime)[7];
    A64_flip[1 + offset] = ((uint8_t *)&Aprime)[6];
    A64_flip[2 + offset] = ((uint8_t *)&Aprime)[5];
    A64_flip[3 + offset] = ((uint8_t *)&Aprime)[4];
    A64_flip[4 + offset] = ((uint8_t *)&Aprime)[3];
    A64_flip[5 + offset] = ((uint8_t *)&Aprime)[2];
    A64_flip[6 + offset] = ((uint8_t *)&Aprime)[1];
    A64_flip[7 + offset] = ((uint8_t *)&Aprime)[0];
    if (crclen == 24)
      crc = (uint64_t)(crc24c(A64_flip, 8 * offset + len) >> 8) & 0xffffff;
    else if (crclen == 11)
      crc = (uint64_t)(crc11(A64_flip, 8 * offset + len) >> 21) & 0x7ff;
    else if (crclen == 6)
      crc = (uint64_t)(crc6(A64_flip, 8 * offset + len) >> 26) & 0x3f;
  }

#ifdef POLAR_CODING_DEBUG
  int A_array = (len + 63) >> 6;
  printf("a: ");
  for (int n = 0; n < len; n++) {
    if (n % 4 == 0) {
      printf(" ");
    }
    int n1 = n >> 6;
    int n2 = n - (n1 << 6);
    int alen = n1 == 0 ? len - (A_array << 6) : 64;
    printf("%lu", (Ar >> (alen - 1 - n2)) & 1);
  }
  printf("\n\n");
#endif

#ifdef POLAR_CODING_DEBUG
  printf("A %lx B %lx|%lx Cprime %lx|%lx (crc %x,rxcrc %lx, XOR %lx, bits%d)\n",
         Ar,
         B[1],
         B[0],
         Cprime[1],
         Cprime[0],
         crc,
         rxcrc,
         crc ^ rxcrc,
         polarParams->payloadBits);
#endif

  out[0] = Ar;

  if (nid) *nid=crc&65535;
  polarReturn(polarParams);
  return crc ^ rxcrc;
}
