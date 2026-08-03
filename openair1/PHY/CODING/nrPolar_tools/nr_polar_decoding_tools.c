/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <assert.h>
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "PHY/sse_intrin.h"

// #define DEBUG_NEW_IMPL 1

static inline void updateBit(uint8_t listSize,
                             uint16_t row,
                             uint16_t col,
                             uint16_t xlen,
                             uint8_t ylen,
                             int zlen,
                             uint8_t bit[xlen][ylen][zlen],
                             uint8_t bitU[xlen][ylen])
{
  const uint offset = (xlen / (pow(2, (ylen - col))));

  for (uint i = 0; i < listSize; i++) {
    if (((row) % (2 * offset)) >= offset) {
      if (bitU[row][col - 1] == 0)
        updateBit(listSize, row, (col - 1), xlen, ylen, zlen, bit, bitU);
      bit[row][col][i] = bit[row][col - 1][i];
    } else {
      if (bitU[row][col - 1] == 0)
        updateBit(listSize, row, (col - 1), xlen, ylen, zlen, bit, bitU);
      if (bitU[row + offset][col - 1] == 0)
        updateBit(listSize, (row + offset), (col - 1), xlen, ylen, zlen, bit, bitU);
      bit[row][col][i] = ((bit[row][col - 1][i] + bit[row + offset][col - 1][i]) % 2);
    }
  }

  bitU[row][col] = 1;
}

static inline void
computeLLR(uint16_t row, uint16_t col, uint8_t i, uint16_t offset, int xlen, int ylen, int zlen, double llr[xlen][ylen][zlen])
{
  double a = llr[row][col + 1][i];
  double b = llr[row + offset][col + 1][i];
  llr[row][col][i] = log((exp(a + b) + 1) / (exp(a) + exp(b))); // eq. (8a)
}

void updateLLR(uint8_t listSize,
               uint16_t row,
               uint16_t col,
               uint16_t xlen,
               uint8_t ylen,
               int zlen,
               double llr[xlen][ylen][zlen],
               uint8_t llrU[xlen][ylen],
               uint8_t bit[xlen][ylen][zlen],
               uint8_t bitU[xlen][ylen])
{
  const uint offset = (xlen / (pow(2, (ylen - col - 1))));
  for (uint i = 0; i < listSize; i++) {
    if ((row % (2 * offset)) >= offset) {
      if (bitU[row - offset][col] == 0)
        updateBit(listSize, (row - offset), col, xlen, ylen, zlen, bit, bitU);
      if (llrU[row - offset][col + 1] == 0)
        updateLLR(listSize, (row - offset), (col + 1), xlen, ylen, zlen, llr, llrU, bit, bitU);
      if (llrU[row][col + 1] == 0)
        updateLLR(listSize, row, (col + 1), xlen, ylen, zlen, llr, llrU, bit, bitU);
      llr[row][col][i] = (pow((-1), bit[row - offset][col][i]) * llr[row - offset][col + 1][i]) + llr[row][col + 1][i];
    } else {
      if (llrU[row][col + 1] == 0)
        updateLLR(listSize, row, (col + 1), xlen, ylen, zlen, llr, llrU, bit, bitU);
      if (llrU[row + offset][col + 1] == 0)
        updateLLR(listSize, (row + offset), (col + 1), xlen, ylen, zlen, llr, llrU, bit, bitU);
      computeLLR(row, col, i, offset, xlen, ylen, zlen, llr);
    }
  }
  llrU[row][col] = 1;

  //	printf("LLR (a %f, b %f): llr[%d][%d] %f\n",32*a,32*b,col,row,32*llr[col][row]);
}

void updatePathMetric(double *pathMetric,
                      uint8_t listSize,
                      uint8_t bitValue,
                      uint16_t row,
                      int xlen,
                      int ylen,
                      int zlen,
                      double llr[xlen][ylen][zlen])
{
  const int multiplier = (2 * bitValue) - 1;
  for (uint i = 0; i < listSize; i++)
    pathMetric[i] += log(1 + exp(multiplier * llr[row][0][i])); // eq. (11b)
}

void updatePathMetric2(double *pathMetric,
                       uint8_t listSize,
                       uint16_t row,
                       int xlen,
                       int ylen,
                       int zlen,
                       double llr[xlen][ylen][zlen])
{
  double tempPM[listSize];
  memcpy(tempPM, pathMetric, (sizeof(double) * listSize));

  uint bitValue = 0;
  int multiplier = (2 * bitValue) - 1;
  for (uint i = 0; i < listSize; i++)
    pathMetric[i] += log(1 + exp(multiplier * llr[row][0][i])); // eq. (11b)

  bitValue = 1;
  multiplier = (2 * bitValue) - 1;
  for (uint i = listSize; i < 2 * listSize; i++)
    pathMetric[i] = tempPM[(i - listSize)] + log(1 + exp(multiplier * llr[row][0][(i - listSize)])); // eq. (11b)
}

static inline decoder_node_t *new_decoder_node(simde__m256i **buffer,
                                               const int level,
                                               const int first_leaf_index,
                                               const bool leaf,
                                               const bool frozen)
{
  int leaf_sz = 1 << level;
  // we will use SIMD128 minimum
  leaf_sz = (leaf_sz + 7) & ~7;
  if (leaf_sz > 15) // we will use SIMD256
    leaf_sz = (leaf_sz + 15) & ~15;
  const int tree_sz = (sizeof(decoder_node_t) + 31) & ~31;
  uintptr_t tmp = (uintptr_t)*buffer;
  tmp = (tmp + sizeof(**buffer) - 1) & ~(sizeof(**buffer) - 1);
  decoder_node_t *node = (decoder_node_t *)tmp;
  *buffer = (simde__m256i *)(tmp + tree_sz + leaf_sz * sizeof(int16_t) + leaf_sz);
  *node = (decoder_node_t){.first_leaf_index = first_leaf_index,
                           .level = level,
                           .leaf = leaf,
                           .all_frozen = frozen,
                           .alpha = tree_sz,
                           .beta = tree_sz + leaf_sz * sizeof(int16_t),
                           .betaInit = false};
  return (node);
}

static inline decoder_node_t *add_nodes(decoder_tree_t *tree,
                                        int level,
                                        int first_leaf_index,
                                        const uint8_t *information_bit_pattern,
                                        simde__m256i **buffer)
{
  if (level == 0) {
#ifdef DEBUG_NEW_IMPL
    printf("leaf %d (%s)\n", first_leaf_index, information_bit_pattern[first_leaf_index] == 1 ? "information or crc" : "frozen");
#endif
    return new_decoder_node(buffer, level, first_leaf_index, true, information_bit_pattern[first_leaf_index] == 0);
  }

  int Nv = 1 << level;
  const uint8_t *ptr = information_bit_pattern + first_leaf_index;
  const uint8_t *end = ptr + Nv;
  bool all_frozen_below = true;
  while (ptr < end && all_frozen_below) {
    all_frozen_below = *ptr++ <= 0;
  }

  if (all_frozen_below) {
    return new_decoder_node(buffer, level, first_leaf_index, true, true);
  }

  decoder_node_t *new_node = new_decoder_node(buffer, level, first_leaf_index, false, false);
  new_node->left = add_nodes(tree, level - 1, first_leaf_index, information_bit_pattern, buffer);
  new_node->right = add_nodes(tree, level - 1, first_leaf_index + (Nv / 2), information_bit_pattern, buffer);
  return (new_node);
}

void build_decoder_tree(t_nrPolar_params *pp)
{
  uintptr_t tmp = (uintptr_t)pp->decoder.buffer;
  tmp = (tmp + sizeof(*pp->decoder.buffer) - 1) & ~(sizeof(*pp->decoder.buffer) - 1);
  simde__m256i *buffer = (simde__m256i *)tmp;
  pp->decoder.root = add_nodes(&pp->decoder, pp->n, 0, pp->information_bit_pattern, &buffer);
  AssertFatal(buffer < pp->decoder.buffer + sizeofArray(pp->decoder.buffer),
              "Arbitrary size too small (also check arbitrary max size of the entire polar decoder) %ld instead of max %ld\n",
              buffer - pp->decoder.buffer,
              sizeofArray(pp->decoder.buffer));
}

static inline void applyFtoleft(const t_nrPolar_params *pp, decoder_node_t *node, uint8_t *output)
{
  int16_t *alpha_v = treeAlpha(node);
  int16_t *alpha_l = treeAlpha(node->left);
  int8_t *betal = treeBeta(node->left);

#ifdef DEBUG_NEW_IMPL
  printf("applyFtoleft %d, Nv %d (level %d,node->left (leaf %d, AF %d))\n",
         node->first_leaf_index,
         node->Nv,
         node->level,
         node->left->leaf,
         node->left->all_frozen);

  for (int i = 0; i < node->Nv; i++)
    printf("i%d (frozen %d): alpha_v[i] = %d\n", i, 1 - pp->information_bit_pattern[node->first_leaf_index + i], alpha_v[i]);
#endif
  assert(node->level);
  const int sz = 1 << (node->level - 1);
  if (node->left->all_frozen == 0) {
    int avx2mod = sz & 15;
    if (avx2mod == 0) {
      int avx2len = sz / 16;

      //      printf("avx2len %d\n",avx2len);
      for (int i = 0; i < avx2len; i++) {
        simde__m256i a256 = ((simde__m256i *)alpha_v)[i];
        simde__m256i b256 = ((simde__m256i *)alpha_v)[i + avx2len];
        simde__m256i absa256 = simde_mm256_abs_epi16(a256);
        simde__m256i absb256 = simde_mm256_abs_epi16(b256);
        simde__m256i minabs256 = simde_mm256_min_epi16(absa256, absb256);
        ((simde__m256i *)alpha_l)[i] = simde_mm256_sign_epi16(minabs256, simde_mm256_sign_epi16(a256, b256));
      }
    } else if (avx2mod == 8) {
      simde__m128i a128 = *((simde__m128i *)alpha_v);
      simde__m128i b128 = ((simde__m128i *)alpha_v)[1];
      simde__m128i absa128 = simde_mm_abs_epi16(a128);
      simde__m128i absb128 = simde_mm_abs_epi16(b128);
      simde__m128i minabs128 = simde_mm_min_epi16(absa128, absb128);
      *((simde__m128i *)alpha_l) = simde_mm_sign_epi16(minabs128, simde_mm_sign_epi16(a128, b128));
    } else if (avx2mod == 4) {
      // this uses __m64, but only with SSE instructions, so we can disable MMX even with this piece of code
      simde__m64 a64 = *((simde__m64 *)alpha_v);
      simde__m64 b64 = ((simde__m64 *)alpha_v)[1];
      simde__m64 absa64 = simde_mm_abs_pi16(a64);
      simde__m64 absb64 = simde_mm_abs_pi16(b64);
      simde__m64 minabs64 = simde_mm_min_pi16(absa64, absb64);
      *((simde__m64 *)alpha_l) = simde_mm_sign_pi16(minabs64, simde_mm_sign_pi16(a64, b64));
    } else { // equivalent scalar code to above, activated only on non x86/ARM architectures
      for (int i = 0; i < sz; i++) {
        int16_t a = alpha_v[i];
        int16_t b = alpha_v[i + sz];
        int16_t maska = a >> 15;
        int16_t maskb = b >> 15;
        int16_t absa = (a + maska) ^ maska;
        int16_t absb = (b + maskb) ^ maskb;
        int16_t minabs = absa < absb ? absa : absb;
        alpha_l[i] = (maska ^ maskb) == 0 ? minabs : -minabs;
        //	printf("alphal[%d] %d (%d,%d)\n",i,alpha_l[i],a,b);
      }
    }
    if (sz == 1) { // apply hard decision on left node
      const int tmp = alpha_l[0] <= 0;
      betal[0] = tmp * 2 - 1;
      node->left->betaInit = true;
#ifdef DEBUG_NEW_IMPL
      printf("betal[0] %d (%p)\n", betal[0], &betal[0]);
#endif
      output[node->first_leaf_index] = tmp;
#ifdef DEBUG_NEW_IMPL
      printf("Setting bit %d to %d (LLR %d)\n", node->first_leaf_index, (betal[0] + 1) >> 1, alpha_l[0]);
#endif
    }
  }
}

static inline void applyGtoright(const t_nrPolar_params *pp, decoder_node_t *node, uint8_t *output)
{
  int16_t *alpha_v = treeAlpha(node);
  int16_t *alpha_r = treeAlpha(node->right);
  int8_t *betal = treeBeta(node->left);
  int8_t *betar = treeBeta(node->right);

#ifdef DEBUG_NEW_IMPL
  printf("applyGtoright %d, Nv %d (level %d), (leaf %d, AF %d)\n",
         node->first_leaf_index,
         node->Nv,
         node->level,
         node->right->leaf,
         node->right->all_frozen);
#endif
  assert(node->level);
  const int sz = 1 << (node->level - 1);
  if (node->right->all_frozen == 0) {
    if (!node->left->betaInit) {
      int avx2mod = sz & 15;
      if (avx2mod == 0) {
        int avx2len = sz / 16;
        for (int i = 0; i < avx2len; i++) {
          ((simde__m256i *)alpha_r)[i] =
              simde_mm256_adds_epi16(((simde__m256i *)alpha_v)[i + avx2len], ((simde__m256i *)alpha_v)[i]);
        }
      } else if (avx2mod == 8) {
        ((simde__m128i *)alpha_r)[0] = simde_mm_adds_epi16(((simde__m128i *)alpha_v)[1], ((simde__m128i *)alpha_v)[0]);
      } else {
        for (int i = 0; i < sz; i++) {
          int temp_alpha_r = alpha_v[i + sz] + alpha_v[i];
          if (temp_alpha_r > SHRT_MAX) {
            alpha_r[i] = SHRT_MAX;
          } else if (temp_alpha_r < -SHRT_MAX) {
            alpha_r[i] = -SHRT_MAX;
          } else {
            alpha_r[i] = temp_alpha_r;
          }
        }
      }
    } else {
      int avx2mod = sz & 15;
      if (avx2mod == 0) {
        int avx2len = sz / 16;

        for (int i = 0; i < avx2len; i++) {
          simde__m256i tmp =
              simde_mm256_sign_epi16(((simde__m256i *)alpha_v)[i], simde_mm256_cvtepi8_epi16(((simde__m128i *)betal)[i]));
          ((simde__m256i *)alpha_r)[i] = simde_mm256_subs_epi16(((simde__m256i *)alpha_v)[i + avx2len], tmp);
        }
      } else if (avx2mod == 8) {
        simde__m128i tmp = simde_mm_sign_epi16(((simde__m128i *)alpha_v)[0], simde_mm_cvtepi8_epi16(((simde__m128i *)betal)[0]));
        ((simde__m128i *)alpha_r)[0] = simde_mm_subs_epi16(((simde__m128i *)alpha_v)[1], tmp);
      } else {
        simde__m128i tmp = simde_mm_sign_epi16(*(simde__m128i *)alpha_v, simde_mm_cvtepi8_epi16(*(simde__m128i *)betal));
        simde__m128i tmp2 = simde_mm_loadu_si128((simde__m128i *)(alpha_v + sz));
        simde__m128i res = simde_mm_subs_epi16(tmp2, tmp);
        simde_mm_storeu_si128(alpha_r, res);
      }
    }
    if (sz == 1) { // apply hard decision on right node
      int tmp = alpha_r[0] <= 0;
      betar[0] = tmp * 2 - 1;
      node->right->betaInit = true;
      output[node->first_leaf_index + 1] = tmp;

#ifdef DEBUG_NEW_IMPL
      printf("Setting bit %d to %d (LLR %d)\n", node->first_leaf_index + 1, (betar[0] + 1) >> 1, alpha_r[0]);
#endif
    }
  }
}

static const int64_t all11[4] = {-1, -1, -1, -1};

static inline void computeBeta(decoder_node_t *node)
{
  int8_t *betav = treeBeta(node);
  int8_t *betal = treeBeta(node->left);
  int8_t *betar = treeBeta(node->right);
#ifdef DEBUG_NEW_IMPL
  printf("Computing beta @ level %d first_leaf_index %d (all_frozen %d)\n",
         node->level,
         node->first_leaf_index,
         node->left->all_frozen);
#endif
  assert(node->level);
  const int sz = 1 << (node->level - 1);
  if (node->left->all_frozen == 0) { // if left node is not aggregation of frozen bits
    if (!node->left->betaInit) {
      memset(betal, -1, sz);
      node->left->betaInit = true;
    }
    if (!node->right->betaInit) {
      memset(betar, -1, sz);
      node->right->betaInit = true;
    }
    int avx2mod = sz & 31;
    if (avx2mod == 0) {
      int avx2len = sz / 16;
      for (int i = 0; i < avx2len; i++) {
        ((simde__m256i *)betav)[i] =
            simde_mm256_xor_si256(simde_mm256_xor_si256(((simde__m256i *)betar)[i], ((simde__m256i *)betal)[i]),
                                  *(simde__m256i *)all11);
      }
    } else {
      ((simde__m128i *)betav)[0] =
          simde_mm_xor_si128(simde_mm_xor_si128(((simde__m128i *)betar)[0], ((simde__m128i *)betal)[0]), *((simde__m128i *)all11));
    }
  } else {
    assert(node->right->betaInit);
    memcpy(betav, betar, sz * sizeof(*betav));
    node->betaInit = true;
  }
  assert(node->right->betaInit);
  if (sz == 1)
    betav[sz] = *betar;
  else
    memcpy(&betav[sz], betar, sz * sizeof(*betav));
  node->betaInit = true;
}

void generic_polar_decoder_recursive(t_nrPolar_params *pp, decoder_node_t *node, uint8_t *nr_polar_U)
{
  // Apply F to left
  applyFtoleft(pp, node, nr_polar_U);
  int iter = pp->tree_linearization.iter++;
  pp->tree_linearization.op_list[iter].op_code = POLAR_OP_CODE_LEFT;
  pp->tree_linearization.op_list[iter].node = node;

  // if left is not a leaf recurse down to the left
  if (node->left->leaf == 0)
    generic_polar_decoder_recursive(pp, node->left, nr_polar_U);

  applyGtoright(pp, node, nr_polar_U);
  iter = pp->tree_linearization.iter++;
  pp->tree_linearization.op_list[iter].op_code = POLAR_OP_CODE_RIGHT;
  pp->tree_linearization.op_list[iter].node = node;

  if (node->right->leaf == 0)
    generic_polar_decoder_recursive(pp, node->right, nr_polar_U);

  computeBeta(node);

  iter = pp->tree_linearization.iter++;
  pp->tree_linearization.op_list[iter].op_code = POLAR_OP_CODE_BETA;
  pp->tree_linearization.op_list[iter].node = node;
}

void generic_polar_decoder(t_nrPolar_params *pp, decoder_node_t *node, uint8_t *nr_polar_U)
{
  if (pp->tree_linearization.is_initialized == false) {
    pp->tree_linearization.iter = 0;
    generic_polar_decoder_recursive(pp, node, nr_polar_U);
    pp->tree_linearization.is_initialized = true;
    AssertFatal(pp->tree_linearization.iter < sizeofArray(pp->tree_linearization.op_list),
                "Arbitrary size %lu is too small, number of operations is %d",
                sizeofArray(pp->tree_linearization.op_list),
                pp->tree_linearization.iter);
  } else {
    for (int i = 0; i < pp->tree_linearization.iter; i++) {
      decoder_node_t *n = pp->tree_linearization.op_list[i].node;
      switch (pp->tree_linearization.op_list[i].op_code) {
        case POLAR_OP_CODE_LEFT:
          applyFtoleft(pp, n, nr_polar_U);
          break;
        case POLAR_OP_CODE_RIGHT:
          applyGtoright(pp, n, nr_polar_U);
          break;
        case POLAR_OP_CODE_BETA:
          computeBeta(n);
      }
    }
  }
}
