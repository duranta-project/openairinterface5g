
/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the functions for check node processing
 */

#ifndef __NR_LDPC_DECODER_CNPROC__H__
#define __NR_LDPC_DECODER_CNPROC__H__

#include "PHY/sse_intrin.h"
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

/**
   \brief Performs CN processing for BG2 on the CN processing buffer and stores the results in the CN processing results buffer.
   \param p_lut Pointer to decoder LUTs
   \param p_procBuf Pointer to processing buffers
   \param Z Lifting size
*/

#if defined(__AVX512BW__)
#include "nrLDPC_cnProc_avx512.h"
#else
static inline void nrLDPC_cnProc_BG2(t_nrLDPC_lut* p_lut, int8_t* cnProcBuf, int8_t* cnProcBufRes, uint16_t Z)
{
    const uint8_t*  lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t* lut_startAddrCnGroups = p_lut->startAddrCnGroups;

    simde__m256i* p_cnProcBuf;
    simde__m256i* p_cnProcBufRes;

    // Number of CNs in Groups
    uint32_t M;
    uint32_t i;
    uint32_t j;
    uint32_t k;
    // Offset to each bit within a group in terms of 32 Byte
    uint32_t bitOffsetInGroup;

    simde__m256i ymm0, min, sgn;
    simde__m256i* p_cnProcBufResBit;

    const simde__m256i* p_ones   = (simde__m256i*) ones256_epi8;
    const simde__m256i* p_maxLLR = (simde__m256i*) maxLLR256_epi8;

    // LUT with offsets for bits that need to be processed
    // 1. bit proc requires LLRs of 2. and 3. bit, 2.bits of 1. and 3. etc.
    // Offsets are in units of bitOffsetInGroup
    const uint8_t lut_idxCnProcG3[3][2] = {{72,144}, {0,144}, {0,72}};

    // =====================================================================
    // Process group with 3 BNs

    if (lut_numCnInCnGroups[0] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[0]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[0]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 3
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[0]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[0]];

        // Loop over every BN
        for (j=0; j<3; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            simde__m256i *pj0 = &p_cnProcBuf[lut_idxCnProcG3[j][0]];
            simde__m256i *pj1 = &p_cnProcBuf[lut_idxCnProcG3[j][1]];

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
              //                ymm0 = p_cnProcBuf[lut_idxCnProcG3[j][0] + i];
                ymm0 = pj0[i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // 32 CNs of second BN
                //  ymm0 = p_cnProcBuf[lut_idxCnProcG3[j][1] + i];
                ymm0 = pj1[i];
                min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                sgn  = simde_mm256_sign_epi8(sgn, ymm0);

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                //*p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                //p_cnProcBufResBit++;
                p_cnProcBufResBit[i]=simde_mm256_sign_epi8(min, sgn);
            }
        }
    }

    // =====================================================================
    // Process group with 4 BNs

    // Offset is 20*384/32 = 240
    const uint16_t lut_idxCnProcG4[4][3] = {{240,480,720}, {0,480,720}, {0,240,720}, {0,240,480}};

    if (lut_numCnInCnGroups[1] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[1]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[1]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 4
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[1]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[1]];

        // Loop over every BN
        for (j=0; j<4; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG4[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<3; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG4[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 5 BNs

    // Offset is 9*384/32 = 108
    const uint16_t lut_idxCnProcG5[5][4] = {{108,216,324,432}, {0,216,324,432},
                                            {0,108,324,432}, {0,108,216,432}, {0,108,216,324}};

    if (lut_numCnInCnGroups[2] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[2]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[2]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 5
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[2]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[2]];

        // Loop over every BN
        for (j=0; j<5; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG5[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<4; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG5[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 6 BNs

    // Offset is 3*384/32 = 36
    const uint16_t lut_idxCnProcG6[6][5] = {{36,72,108,144,180}, {0,72,108,144,180},
                                            {0,36,108,144,180}, {0,36,72,144,180},
                                            {0,36,72,108,180}, {0,36,72,108,144}};

    if (lut_numCnInCnGroups[3] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[3]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[3]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 6
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[3]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[3]];

        // Loop over every BN
        for (j=0; j<6; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG6[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<5; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG6[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 8 BNs

    // Offset is 2*384/32 = 24
    const uint8_t lut_idxCnProcG8[8][7] = {{24,48,72,96,120,144,168}, {0,48,72,96,120,144,168},
                                           {0,24,72,96,120,144,168}, {0,24,48,96,120,144,168},
                                           {0,24,48,72,120,144,168}, {0,24,48,72,96,144,168},
                                           {0,24,48,72,96,120,168}, {0,24,48,72,96,120,144}};

    if (lut_numCnInCnGroups[4] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[4]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[4]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 8
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[4]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[4]];

        // Loop over every BN
        for (j=0; j<8; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG8[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<7; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG8[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 10 BNs

    // Offset is 2*384/32 = 24
    const uint8_t lut_idxCnProcG10[10][9] = {{24,48,72,96,120,144,168,192,216}, {0,48,72,96,120,144,168,192,216},
                                             {0,24,72,96,120,144,168,192,216}, {0,24,48,96,120,144,168,192,216},
                                             {0,24,48,72,120,144,168,192,216}, {0,24,48,72,96,144,168,192,216},
                                             {0,24,48,72,96,120,168,192,216}, {0,24,48,72,96,120,144,192,216},
                                             {0,24,48,72,96,120,144,168,216}, {0,24,48,72,96,120,144,168,192}};

    if (lut_numCnInCnGroups[5] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[5]*Z + 31)>>5;
        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG2_R15[5]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 10
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[5]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[5]];

        // Loop over every BN
        for (j=0; j<10; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG10[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<9; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG10[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

}


/**
   \brief Performs CN processing for BG1 on the CN processing buffer and stores the results in the CN processing results buffer.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
*/



/**
   \brief Performs CN processing for BG1 on the CN processing buffer and stores the results in the CN processing results buffer.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
*/
static inline void nrLDPC_cnProc_BG1(t_nrLDPC_lut* p_lut, int8_t* cnProcBuf, int8_t* cnProcBufRes, uint16_t Z)
{
    const uint8_t*  lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t* lut_startAddrCnGroups = p_lut->startAddrCnGroups;

    simde__m256i* p_cnProcBuf;
    simde__m256i* p_cnProcBufRes;

    // Number of CNs in Groups
    uint32_t M;
    uint32_t i;
    uint32_t j;
    uint32_t k;
    // Offset to each bit within a group in terms of 32 Byte
    uint32_t bitOffsetInGroup;

    simde__m256i ymm0, min, sgn;
    simde__m256i* p_cnProcBufResBit;

    const simde__m256i* p_ones   = (simde__m256i*) ones256_epi8;
    const simde__m256i* p_maxLLR = (simde__m256i*) maxLLR256_epi8;

    // LUT with offsets for bits that need to be processed
    // 1. bit proc requires LLRs of 2. and 3. bit, 2.bits of 1. and 3. etc.
    // Offsets are in units of bitOffsetInGroup (1*384/32)
    const uint8_t lut_idxCnProcG3[3][2] = {{12,24}, {0,24}, {0,12}};

    // =====================================================================
    // Process group with 3 BNs

    if (lut_numCnInCnGroups[0] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[0]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[0]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 3
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[0]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[0]];

        // Loop over every BN
        for (j=0; j<3; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG3[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // 32 CNs of second BN
                ymm0 = p_cnProcBuf[lut_idxCnProcG3[j][1] + i];
                min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                sgn  = simde_mm256_sign_epi8(sgn, ymm0);


                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 4 BNs

    // Offset is 5*384/32 = 60
    const uint8_t lut_idxCnProcG4[4][3] = {{60,120,180}, {0,120,180}, {0,60,180}, {0,60,120}};

    if (lut_numCnInCnGroups[1] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[1]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[1]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 4
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[1]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[1]];

        // Loop over every BN
        for (j=0; j<4; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG4[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<3; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG4[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 5 BNs

    // Offset is 18*384/32 = 216
    const uint16_t lut_idxCnProcG5[5][4] = {{216,432,648,864}, {0,432,648,864},
                                            {0,216,648,864}, {0,216,432,864}, {0,216,432,648}};

    if (lut_numCnInCnGroups[2] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[2]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[2]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 5
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[2]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[2]];

        // Loop over every BN
        for (j=0; j<5; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG5[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<4; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG5[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 6 BNs

    // Offset is 8*384/32 = 96
    const uint16_t lut_idxCnProcG6[6][5] = {{96,192,288,384,480}, {0,192,288,384,480},
                                            {0,96,288,384,480}, {0,96,192,384,480},
                                            {0,96,192,288,480}, {0,96,192,288,384}};

    if (lut_numCnInCnGroups[3] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[3]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[3]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 6
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[3]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[3]];

        // Loop over every BN
        for (j=0; j<6; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG6[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<5; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG6[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 7 BNs

    // Offset is 5*384/32 = 60
    const uint16_t lut_idxCnProcG7[7][6] = {{60,120,180,240,300,360}, {0,120,180,240,300,360},
                                            {0,60,180,240,300,360},   {0,60,120,240,300,360},
                                            {0,60,120,180,300,360},   {0,60,120,180,240,360},
                                            {0,60,120,180,240,300}};

    if (lut_numCnInCnGroups[4] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[4]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[4]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 7
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[4]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[4]];

        // Loop over every BN
        for (j=0; j<7; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG7[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<6; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG7[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 8 BNs

    // Offset is 2*384/32 = 24
    const uint8_t lut_idxCnProcG8[8][7] = {{24,48,72,96,120,144,168}, {0,48,72,96,120,144,168},
                                           {0,24,72,96,120,144,168}, {0,24,48,96,120,144,168},
                                           {0,24,48,72,120,144,168}, {0,24,48,72,96,144,168},
                                           {0,24,48,72,96,120,168}, {0,24,48,72,96,120,144}};

    if (lut_numCnInCnGroups[5] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[5]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[5]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 8
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[5]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[5]];

        // Loop over every BN
        for (j=0; j<8; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG8[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<7; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG8[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 9 BNs

    // Offset is 2*384/32 = 24
    const uint8_t lut_idxCnProcG9[9][8] = {{24,48,72,96,120,144,168,192}, {0,48,72,96,120,144,168,192},
                                           {0,24,72,96,120,144,168,192}, {0,24,48,96,120,144,168,192},
                                           {0,24,48,72,120,144,168,192}, {0,24,48,72,96,144,168,192},
                                           {0,24,48,72,96,120,168,192}, {0,24,48,72,96,120,144,192},
                                           {0,24,48,72,96,120,144,168}};

    if (lut_numCnInCnGroups[6] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[6]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[6]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 9
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[6]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[6]];

        // Loop over every BN
        for (j=0; j<9; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG9[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<8; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG9[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 10 BNs

    // Offset is 1*384/32 = 12
    const uint8_t lut_idxCnProcG10[10][9] = {{12,24,36,48,60,72,84,96,108}, {0,24,36,48,60,72,84,96,108},
                                             {0,12,36,48,60,72,84,96,108}, {0,12,24,48,60,72,84,96,108},
                                             {0,12,24,36,60,72,84,96,108}, {0,12,24,36,48,72,84,96,108},
                                             {0,12,24,36,48,60,84,96,108}, {0,12,24,36,48,60,72,96,108},
                                             {0,12,24,36,48,60,72,84,108}, {0,12,24,36,48,60,72,84,96}};

    if (lut_numCnInCnGroups[7] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[7]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[7]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 10
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[7]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[7]];

        // Loop over every BN
        for (j=0; j<10; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG10[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<9; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG10[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

    // =====================================================================
    // Process group with 19 BNs

    // Offset is 4*384/32 = 12
    const uint16_t lut_idxCnProcG19[19][18] = {{48,96,144,192,240,288,336,384,432,480,528,576,624,672,720,768,816,864}, {0,96,144,192,240,288,336,384,432,480,528,576,624,672,720,768,816,864},
                                               {0,48,144,192,240,288,336,384,432,480,528,576,624,672,720,768,816,864}, {0,48,96,192,240,288,336,384,432,480,528,576,624,672,720,768,816,864},
                                               {0,48,96,144,240,288,336,384,432,480,528,576,624,672,720,768,816,864}, {0,48,96,144,192,288,336,384,432,480,528,576,624,672,720,768,816,864},
                                               {0,48,96,144,192,240,336,384,432,480,528,576,624,672,720,768,816,864}, {0,48,96,144,192,240,288,384,432,480,528,576,624,672,720,768,816,864},
                                               {0,48,96,144,192,240,288,336,432,480,528,576,624,672,720,768,816,864}, {0,48,96,144,192,240,288,336,384,480,528,576,624,672,720,768,816,864},
                                               {0,48,96,144,192,240,288,336,384,432,528,576,624,672,720,768,816,864}, {0,48,96,144,192,240,288,336,384,432,480,576,624,672,720,768,816,864},
                                               {0,48,96,144,192,240,288,336,384,432,480,528,624,672,720,768,816,864}, {0,48,96,144,192,240,288,336,384,432,480,528,576,672,720,768,816,864},
                                               {0,48,96,144,192,240,288,336,384,432,480,528,576,624,720,768,816,864}, {0,48,96,144,192,240,288,336,384,432,480,528,576,624,672,768,816,864},
                                               {0,48,96,144,192,240,288,336,384,432,480,528,576,624,672,720,816,864}, {0,48,96,144,192,240,288,336,384,432,480,528,576,624,672,720,768,864},
                                               {0,48,96,144,192,240,288,336,384,432,480,528,576,624,672,720,768,816}};

    if (lut_numCnInCnGroups[8] > 0)
    {
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M = (lut_numCnInCnGroups[8]*Z + 31)>>5;

        // Set the offset to each bit within a group in terms of 32 Byte
        bitOffsetInGroup = (lut_numCnInCnGroups_BG1_R13[8]*NR_LDPC_ZMAX)>>5;

        // Set pointers to start of group 19
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[8]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[8]];

        // Loop over every BN
        for (j=0; j<19; j++)
        {
            // Set of results pointer to correct BN address
            p_cnProcBufResBit = p_cnProcBufRes + (j*bitOffsetInGroup);

            // Loop over CNs
            for (i=0; i<M; i++)
            {
                // Abs and sign of 32 CNs (first BN)
                ymm0 = p_cnProcBuf[lut_idxCnProcG19[j][0] + i];
                sgn  = simde_mm256_sign_epi8(*p_ones, ymm0);
                min  = simde_mm256_abs_epi8(ymm0);

                // Loop over BNs
                for (k=1; k<18; k++)
                {
                    ymm0 = p_cnProcBuf[lut_idxCnProcG19[j][k] + i];
                    min  = simde_mm256_min_epu8(min, simde_mm256_abs_epi8(ymm0));
                    sgn  = simde_mm256_sign_epi8(sgn, ymm0);
                }

                // Store result
                min = simde_mm256_min_epu8(min, *p_maxLLR); // 128 in epi8 is -127
                *p_cnProcBufResBit = simde_mm256_sign_epi8(min, sgn);
                p_cnProcBufResBit++;
            }
        }
    }

}

#endif /* !__AVX512BW__ — end of generic BG1/BG2 256-bit block */

// =============================================================================
// Two-pass min1/min2 CN processing
//
// Available on ALL targets (AVX512, AVX2, aarch64/NEON, SSE2 fallback).
// On AVX2 / AVX512: uses 256-bit vectors (32 CNs per iteration).
// On aarch64 / SSE2: uses 128-bit vectors (16 CNs per iteration) so that
//   each SIMD operation maps to a single NEON or SSE instruction rather than
//   an emulated pair.
//
// Replaces the LUT-exclude-self approach with a two-pass algorithm:
//
//   Pass 1: read all numBN inputs once to collect:
//             vmin1    — minimum |vk| across all k
//             vmin2    — second minimum |vk|
//             vsgn_xor — XOR of all input bytes; bit 7 = sign parity
//                        (XOR with 0 is identity, so zero-LLR inputs are safe)
//
//   Pass 2: read all numBN inputs a second time; for each k:
//             other_xor = vsgn_xor XOR vk  (removes self via XOR self-inverse)
//             sign_mask = 0xFF where bit 7 of other_xor = 1, else 0x00
//             out_mag   = (|vk| == vmin1) ? vmin2 : vmin1
//             result    = (out_mag XOR sign_mask) - sign_mask  (±out_mag)
//
// Memory reads per CN group: 2 × numBN  vs  numBN × (numBN-1) for LUT approach:
//
//   numBN= 3:  6  vs   6   (break-even)
//   numBN= 4:  8  vs  12   (1.5×  fewer)
//   numBN= 7: 14  vs  42   (3.0×  fewer)
//   numBN=10: 20  vs  90   (4.5×  fewer)
//   numBN=19: 38  vs 342   (9.0×  fewer)
//
// Tie approximation: when two or more BNs share min1, all receive min2 in pass 2.
// This is the standard min-sum tie approximation; effect on convergence is negligible.
// =============================================================================

/* -------------------------------------------------------------------------
 * RVV two-pass group kernel — RISC-V vector (runtime VL).
 * ------------------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
static inline vuint8m1_t nrLDPC_rvv_abs_i8m1(vint8m1_t v, size_t vl)
{
    vbool8_t neg = __riscv_vmslt_vx_i8m1_b8(v, 0, vl);
    vint8m1_t neg_v = __riscv_vneg_v_i8m1(v, vl);
    return __riscv_vreinterpret_v_i8m1_u8m1(__riscv_vmerge_vvm_i8m1(v, neg_v, neg, vl));
}

static inline void nrLDPC_cnProc_group_2pass_rvv(int8_t  *cnProcBuf,
                                                 int8_t  *cnProcBufRes,
                                                 uint32_t numBN,
                                                 uint32_t numBytes,
                                                 uint32_t off)
{
    for (uint32_t i = 0; i < numBytes;) {
        size_t vl = __riscv_vsetvl_e8m1(numBytes - i);
        vuint8m1_t vmin1 = __riscv_vmv_v_x_u8m1(127, vl);
        vuint8m1_t vmin2 = __riscv_vmv_v_x_u8m1(127, vl);
        vint8m1_t vsgn_xor = __riscv_vmv_v_x_i8m1(0, vl);

        for (uint32_t k = 0; k < numBN; k++) {
            vint8m1_t vk = __riscv_vle8_v_i8m1(&cnProcBuf[k * off + i], vl);
            vuint8m1_t vak = nrLDPC_rvv_abs_i8m1(vk, vl);
            vsgn_xor = __riscv_vxor_vv_i8m1(vsgn_xor, vk, vl);
            vuint8m1_t new_min1 = __riscv_vminu_vv_u8m1(vmin1, vak, vl);
            vuint8m1_t new_min2 = __riscv_vminu_vv_u8m1(vmin2, __riscv_vmaxu_vv_u8m1(vmin1, vak, vl), vl);
            vmin1 = new_min1;
            vmin2 = new_min2;
        }

        for (uint32_t k = 0; k < numBN; k++) {
            vint8m1_t vk = __riscv_vle8_v_i8m1(&cnProcBuf[k * off + i], vl);
            vuint8m1_t vak = nrLDPC_rvv_abs_i8m1(vk, vl);
            vbool8_t use_min2 = __riscv_vmseq_vv_u8m1_b8(vak, vmin1, vl);
            vuint8m1_t out_mag = __riscv_vmerge_vvm_u8m1(vmin1, vmin2, use_min2, vl);
            vint8m1_t out_mag_i8 = __riscv_vreinterpret_v_u8m1_i8m1(out_mag);
            vint8m1_t other_xor = __riscv_vxor_vv_i8m1(vsgn_xor, vk, vl);
            vbool8_t neg = __riscv_vmslt_vx_i8m1_b8(other_xor, 0, vl);
            vint8m1_t neg_out = __riscv_vneg_v_i8m1(out_mag_i8, vl);
            __riscv_vse8_v_i8m1(&cnProcBufRes[k * off + i], __riscv_vmerge_vvm_i8m1(out_mag_i8, neg_out, neg, vl), vl);
        }

        i += vl;
    }
}

static inline uint32_t nrLDPC_cnProcPc_group_rvv(const int8_t *cnProcBuf,
                                                 const int8_t *cnProcBufRes,
                                                 uint32_t      numBN,
                                                 uint32_t      numBytes,
                                                 uint32_t      off)
{
    for (uint32_t i = 0; i < numBytes;) {
        size_t vl = __riscv_vsetvl_e8m1(numBytes - i);
        vint8m1_t zero = __riscv_vmv_v_x_i8m1(0, vl);
        vbool8_t pc = __riscv_vmsne_vx_i8m1_b8(zero, 0, vl);

        for (uint32_t j = 0; j < numBN; j++) {
            vint8m1_t in = __riscv_vle8_v_i8m1(&cnProcBuf[j * off + i], vl);
            vint8m1_t res = __riscv_vle8_v_i8m1(&cnProcBufRes[j * off + i], vl);
            vint8m1_t sum = __riscv_vsadd_vv_i8m1(in, res, vl);
            pc = __riscv_vmxor_mm_b8(pc, __riscv_vmslt_vx_i8m1_b8(sum, 0, vl), vl);
        }

        if (__riscv_vcpop_m_b8(pc, vl) != 0)
            return 1;
        i += vl;
    }
    return 0;
}
#endif

/* -------------------------------------------------------------------------
 * 512-bit two-pass group kernel — AVX512BW (64 CNs per iteration).
 * Uses mask-register comparisons and predicated blend for single-cycle
 * select. Only compiled when __AVX512BW__ is defined.
 * ------------------------------------------------------------------------- */
#if defined(__AVX512BW__)
/**
 * \brief Two-pass min-sum CN processor, 512-bit — one degree group.
 *
 * \param cnProcBuf    Start of this group in the CN proc buffer (64-byte aligned).
 * \param cnProcBufRes Start of this group in the CN proc result buffer.
 * \param numBN        Number of BNs per CN in this degree group.
 * \param M            Number of 64-CN chunks: ceil(numCN × Z / 64).
 * \param off          BN-to-BN stride in units of sizeof(simde__m512i) = 64 bytes.
 */
static inline void nrLDPC_cnProc_group_2pass_512(simde__m512i *cnProcBuf,
                                                  simde__m512i *cnProcBufRes,
                                                  uint32_t      numBN,
                                                  uint32_t      M,
                                                  uint32_t      off)
{
    const simde__m512i maxLLR = simde_mm512_set1_epi8(127);
    const simde__m512i zeros  = simde_mm512_setzero_si512();

    for (uint32_t i = 0; i < M; i++) {
        simde__m512i vmin1    = maxLLR;
        simde__m512i vmin2    = maxLLR;
        simde__m512i vsgn_xor = zeros;

        for (uint32_t k = 0; k < numBN; k++) {
            simde__m512i vk  = cnProcBuf[k * off + i];
            simde__m512i vak = simde_mm512_abs_epi8(vk);
            vsgn_xor = simde_mm512_xor_si512(vsgn_xor, vk);
            simde__m512i new_min1 = simde_mm512_min_epu8(vmin1, vak);
            simde__m512i new_min2 = simde_mm512_min_epu8(vmin2,
                                        simde_mm512_max_epu8(vmin1, vak));
            vmin1 = new_min1;
            vmin2 = new_min2;
        }
        for (uint32_t k = 0; k < numBN; k++) {
            simde__m512i   vk        = cnProcBuf[k * off + i];
            simde__m512i   vak       = simde_mm512_abs_epi8(vk);
            simde__mmask64 eq_mask   = simde_mm512_cmpeq_epi8_mask(vak, vmin1);
            simde__m512i   out_mag   = simde_mm512_min_epu8(
                                           simde_mm512_mask_blend_epi8(eq_mask, vmin1, vmin2),
                                           maxLLR);
            simde__m512i   other_xor = simde_mm512_xor_si512(vsgn_xor, vk);
            simde__mmask64 neg_mask  = simde_mm512_cmpgt_epi8_mask(zeros, other_xor);
            simde__m512i   neg_out   = simde_mm512_sub_epi8(zeros, out_mag);
            cnProcBufRes[k * off + i] = simde_mm512_mask_blend_epi8(neg_mask, out_mag, neg_out);
        }
    }
}

/* -------------------------------------------------------------------------
 * 256-bit two-pass group kernel — AVX2 only (32 CNs per iteration).
 * Not compiled on AVX512 targets (512-bit kernel above is used instead)
 * or on pure 128-bit targets (aarch64 NEON, SSE2-only x86).
 * ------------------------------------------------------------------------- */
#elif defined(__AVX2__)
/**
 * \brief Two-pass min-sum CN processor, 256-bit — one degree group.
 *
 * \param cnProcBuf    Start of this group in the CN proc buffer (32-byte aligned).
 * \param cnProcBufRes Start of this group in the CN proc result buffer.
 * \param numBN        Number of BNs per CN in this degree group.
 * \param M            Number of 32-CN chunks: ceil(numCN × Z / 32).
 * \param off          BN-to-BN stride in units of sizeof(simde__m256i) = 32 bytes.
 */
static inline void nrLDPC_cnProc_group_2pass(simde__m256i *cnProcBuf,
                                             simde__m256i *cnProcBufRes,
                                             uint32_t      numBN,
                                             uint32_t      M,
                                             uint32_t      off)
{
    const simde__m256i maxLLR  = *(const simde__m256i *)maxLLR256_epi8;
    const simde__m256i zeros   = simde_mm256_setzero_si256();

    for (uint32_t i = 0; i < M; i++) {
        simde__m256i vmin1    = maxLLR;
        simde__m256i vmin2    = maxLLR;
        simde__m256i vsgn_xor = zeros;

        for (uint32_t k = 0; k < numBN; k++) {
            simde__m256i vk  = cnProcBuf[k * off + i];
            simde__m256i vak = simde_mm256_abs_epi8(vk);
            vsgn_xor = simde_mm256_xor_si256(vsgn_xor, vk);
            simde__m256i new_min1 = simde_mm256_min_epu8(vmin1, vak);
            simde__m256i new_min2 = simde_mm256_min_epu8(vmin2,
                                        simde_mm256_max_epu8(vmin1, vak));
            vmin1 = new_min1;
            vmin2 = new_min2;
        }
        for (uint32_t k = 0; k < numBN; k++) {
            simde__m256i vk      = cnProcBuf[k * off + i];
            simde__m256i vak     = simde_mm256_abs_epi8(vk);
            simde__m256i mask    = simde_mm256_cmpeq_epi8(vak, vmin1);
            simde__m256i out_mag = simde_mm256_min_epu8(
                                       simde_mm256_blendv_epi8(vmin1, vmin2, mask),
                                       maxLLR);
            simde__m256i other_xor = simde_mm256_xor_si256(vsgn_xor, vk);
            simde__m256i sign_mask = simde_mm256_cmpgt_epi8(zeros, other_xor);
            cnProcBufRes[k * off + i] = simde_mm256_sub_epi8(
                                            simde_mm256_xor_si256(out_mag, sign_mask),
                                            sign_mask);
        }
    }
}
#endif /* __AVX512BW__ / __AVX2__ */

/* -------------------------------------------------------------------------
 * 128-bit two-pass group kernel — aarch64 NEON and SSE2/SSSE3 fallback
 * (16 CNs per iteration; each simde__m128i op = one NEON or SSE instruction).
 * Always compiled so it is available on every target.
 * ------------------------------------------------------------------------- */
/**
 * \brief Two-pass min-sum CN processor, 128-bit — one degree group.
 *
 * \param cnProcBuf    Start of this group in the CN proc buffer (128-bit aligned).
 * \param cnProcBufRes Start of this group in the CN proc result buffer.
 * \param numBN        Number of BNs per CN in this degree group.
 * \param M            Number of 16-CN chunks: ceil(numCN × Z / 16).
 * \param off          BN-to-BN stride in units of sizeof(simde__m128i) = 16 bytes.
 */
static inline void nrLDPC_cnProc_group_2pass_128(simde__m128i *cnProcBuf,
                                                 simde__m128i *cnProcBufRes,
                                                 uint32_t      numBN,
                                                 uint32_t      M,
                                                 uint32_t      off)
{
    const simde__m128i maxLLR  = *(const simde__m128i *)maxLLR256_epi8;
    const simde__m128i zeros   = simde_mm_setzero_si128();

    for (uint32_t i = 0; i < M; i++) {
        simde__m128i vmin1    = maxLLR;
        simde__m128i vmin2    = maxLLR;
        simde__m128i vsgn_xor = zeros;

        for (uint32_t k = 0; k < numBN; k++) {
            simde__m128i vk  = cnProcBuf[k * off + i];
            simde__m128i vak = simde_mm_abs_epi8(vk);
            vsgn_xor = simde_mm_xor_si128(vsgn_xor, vk);
            simde__m128i new_min1 = simde_mm_min_epu8(vmin1, vak);
            simde__m128i new_min2 = simde_mm_min_epu8(vmin2,
                                        simde_mm_max_epu8(vmin1, vak));
            vmin1 = new_min1;
            vmin2 = new_min2;
        }
        for (uint32_t k = 0; k < numBN; k++) {
            simde__m128i vk      = cnProcBuf[k * off + i];
            simde__m128i vak     = simde_mm_abs_epi8(vk);
            simde__m128i mask    = simde_mm_cmpeq_epi8(vak, vmin1);
            simde__m128i out_mag = simde_mm_min_epu8(
                                       simde_mm_blendv_epi8(vmin1, vmin2, mask),
                                       maxLLR);
            simde__m128i other_xor = simde_mm_xor_si128(vsgn_xor, vk);
            simde__m128i sign_mask = simde_mm_cmpgt_epi8(zeros, other_xor);
            cnProcBufRes[k * off + i] = simde_mm_sub_epi8(
                                            simde_mm_xor_si128(out_mag, sign_mask),
                                            sign_mask);
        }
    }
}

/* -------------------------------------------------------------------------
 * BG1 / BG2 two-pass wrappers — dispatch to RVV, 512-bit (AVX512BW),
 * 256-bit (AVX2), or 128-bit (aarch64/SSE2) based on compile-time target.
 * ------------------------------------------------------------------------- */

/**
 * \brief Two-pass min-sum CN processing for BG1.
 * Drop-in replacement for nrLDPC_cnProc_BG1; no rate-specific LUT needed.
 */
static inline void nrLDPC_cnProc_BG1_2pass(t_nrLDPC_lut *p_lut,
                                            int8_t       *cnProcBuf,
                                            int8_t       *cnProcBufRes,
                                            uint16_t      Z)
{
    const uint8_t  *lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t *lut_startAddrCnGroups = p_lut->startAddrCnGroups;
    static const uint8_t numBN_per_group[9] = {3, 4, 5, 6, 7, 8, 9, 10, 19};

    for (int grp = 0; grp < 9; grp++) {
        if (lut_numCnInCnGroups[grp] == 0)
            continue;
#if defined(__riscv) && defined(__riscv_vector)
        uint32_t numBytes = (uint32_t)lut_numCnInCnGroups[grp] * Z;
        uint32_t off = lut_numCnInCnGroups_BG1_R13[grp] * NR_LDPC_ZMAX;
        nrLDPC_cnProc_group_2pass_rvv(
            &cnProcBuf   [lut_startAddrCnGroups[grp]],
            &cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], numBytes, off);
#elif defined(__AVX512BW__)
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 63) >> 6;
        uint32_t off = (lut_numCnInCnGroups_BG1_R13[grp] * NR_LDPC_ZMAX) >> 6;
        nrLDPC_cnProc_group_2pass_512(
            (simde__m512i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m512i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#elif defined(__AVX2__)
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 31) >> 5;
        uint32_t off = (lut_numCnInCnGroups_BG1_R13[grp] * NR_LDPC_ZMAX) >> 5;
        nrLDPC_cnProc_group_2pass(
            (simde__m256i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m256i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#else
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 15) >> 4;
        uint32_t off = (lut_numCnInCnGroups_BG1_R13[grp] * NR_LDPC_ZMAX) >> 4;
        nrLDPC_cnProc_group_2pass_128(
            (simde__m128i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m128i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#endif
    }
}

/**
 * \brief Two-pass min-sum CN processing for BG2.
 * Drop-in replacement for nrLDPC_cnProc_BG2; no rate-specific LUT needed.
 */
static inline void nrLDPC_cnProc_BG2_2pass(t_nrLDPC_lut *p_lut,
                                            int8_t       *cnProcBuf,
                                            int8_t       *cnProcBufRes,
                                            uint16_t      Z)
{
    const uint8_t  *lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t *lut_startAddrCnGroups = p_lut->startAddrCnGroups;
    static const uint8_t numBN_per_group[6] = {3, 4, 5, 6, 8, 10};

    for (int grp = 0; grp < 6; grp++) {
        if (lut_numCnInCnGroups[grp] == 0)
            continue;
#if defined(__riscv) && defined(__riscv_vector)
        uint32_t numBytes = (uint32_t)lut_numCnInCnGroups[grp] * Z;
        uint32_t off = lut_numCnInCnGroups_BG2_R15[grp] * NR_LDPC_ZMAX;
        nrLDPC_cnProc_group_2pass_rvv(
            &cnProcBuf   [lut_startAddrCnGroups[grp]],
            &cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], numBytes, off);
#elif defined(__AVX512BW__)
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 63) >> 6;
        uint32_t off = (lut_numCnInCnGroups_BG2_R15[grp] * NR_LDPC_ZMAX) >> 6;
        nrLDPC_cnProc_group_2pass_512(
            (simde__m512i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m512i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#elif defined(__AVX2__)
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 31) >> 5;
        uint32_t off = (lut_numCnInCnGroups_BG2_R15[grp] * NR_LDPC_ZMAX) >> 5;
        nrLDPC_cnProc_group_2pass(
            (simde__m256i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m256i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#else
        uint32_t M   = ((uint32_t)lut_numCnInCnGroups[grp] * Z + 15) >> 4;
        uint32_t off = (lut_numCnInCnGroups_BG2_R15[grp] * NR_LDPC_ZMAX) >> 4;
        nrLDPC_cnProc_group_2pass_128(
            (simde__m128i *)&cnProcBuf   [lut_startAddrCnGroups[grp]],
            (simde__m128i *)&cnProcBufRes[lut_startAddrCnGroups[grp]],
            numBN_per_group[grp], M, off);
#endif
    }
}

/**
   \brief Performs parity check for BG1 on the CN processing buffer. Stops as soon as error is detected.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
   \return 32-bit parity check indicator
*/
static inline uint32_t nrLDPC_cnProcPc_BG1(t_nrLDPC_lut* p_lut, int8_t* cnProcBuf, int8_t* cnProcBufRes, uint16_t Z)
{
#if defined(__riscv) && defined(__riscv_vector)
    const uint8_t *lut_numCnInCnGroups = p_lut->numCnInCnGroups;
    const uint32_t *lut_startAddrCnGroups = p_lut->startAddrCnGroups;
    static const uint8_t numBN_per_group[9] = {3, 4, 5, 6, 7, 8, 9, 10, 19};

    for (int grp = 0; grp < 9; grp++) {
        if (lut_numCnInCnGroups[grp] == 0)
            continue;
        uint32_t numBytes = (uint32_t)lut_numCnInCnGroups[grp] * Z;
        uint32_t off = lut_numCnInCnGroups_BG1_R13[grp] * NR_LDPC_ZMAX;
        if (nrLDPC_cnProcPc_group_rvv(&cnProcBuf[lut_startAddrCnGroups[grp]],
                                      &cnProcBufRes[lut_startAddrCnGroups[grp]],
                                      numBN_per_group[grp], numBytes, off))
            return 1;
    }
    return 0;
#else

    const uint8_t*  lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t* lut_startAddrCnGroups = p_lut->startAddrCnGroups;

    simde__m256i* p_cnProcBuf;
    simde__m256i* p_cnProcBufRes;

    // Number of CNs in Groups
    uint32_t M;
    uint32_t i;
    uint32_t j;
    uint32_t pcRes = 0;
    uint32_t pcResSum = 0;
    uint32_t Mrem;
    uint32_t M32;

    simde__m256i ymm0, ymm1;

    // =====================================================================
    // Process group with 3 BNs

    if (lut_numCnInCnGroups[0] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[0]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 3
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[0]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[0]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<3; j++)
            {
                // BN offset is units of (1*384/32) = 12
                ymm0 = p_cnProcBuf   [j*12 + i];
                ymm1 = p_cnProcBufRes[j*12 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<3; j++)
        {
            // BN offset is units of (1*384/32) = 12
            ymm0 = p_cnProcBuf   [j*12 + i];
            ymm1 = p_cnProcBufRes[j*12 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 4 BNs

    if (lut_numCnInCnGroups[1] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[1]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 4
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[1]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[1]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<4; j++)
            {
                // BN offset is units of 5*384/32 = 60
                ymm0 = p_cnProcBuf   [j*60 + i];
                ymm1 = p_cnProcBufRes[j*60 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<4; j++)
        {
            // BN offset is units of 5*384/32 = 60
            ymm0 = p_cnProcBuf   [j*60 + i];
            ymm1 = p_cnProcBufRes[j*60 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 5 BNs

    if (lut_numCnInCnGroups[2] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[2]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 5
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[2]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[2]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<5; j++)
            {
                // BN offset is units of 18*384/32 = 216
                ymm0 = p_cnProcBuf   [j*216 + i];
                ymm1 = p_cnProcBufRes[j*216 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;

        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<5; j++)
        {
            // BN offset is units of 18*384/32 = 216
            ymm0 = p_cnProcBuf   [j*216 + i];
            ymm1 = p_cnProcBufRes[j*216 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 6 BNs

    if (lut_numCnInCnGroups[3] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[3]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 6
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[3]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[3]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<6; j++)
            {
                // BN offset is units of 8*384/32 = 96
                ymm0 = p_cnProcBuf   [j*96 + i];
                ymm1 = p_cnProcBufRes[j*96 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<6; j++)
        {
            // BN offset is units of 8*384/32 = 96
            ymm0 = p_cnProcBuf   [j*96 + i];
            ymm1 = p_cnProcBufRes[j*96 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 7 BNs

    if (lut_numCnInCnGroups[4] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[4]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 7
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[4]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[4]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<7; j++)
            {
                // BN offset is units of 5*384/32 = 60
                ymm0 = p_cnProcBuf   [j*60 + i];
                ymm1 = p_cnProcBufRes[j*60 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<7; j++)
        {
            // BN offset is units of 5*384/32 = 60
            ymm0 = p_cnProcBuf   [j*60 + i];
            ymm1 = p_cnProcBufRes[j*60 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 8 BNs

    if (lut_numCnInCnGroups[5] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[5]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 8
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[5]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[5]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<8; j++)
            {
                // BN offset is units of 2*384/32 = 24
                ymm0 = p_cnProcBuf   [j*24 + i];
                ymm1 = p_cnProcBufRes[j*24 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<8; j++)
        {
            // BN offset is units of 2*384/32 = 24
            ymm0 = p_cnProcBuf   [j*24 + i];
            ymm1 = p_cnProcBufRes[j*24 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 9 BNs

    if (lut_numCnInCnGroups[6] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[6]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 9
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[6]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[6]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<9; j++)
            {
                // BN offset is units of 2*384/32 = 24
                ymm0 = p_cnProcBuf   [j*24 + i];
                ymm1 = p_cnProcBufRes[j*24 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<9; j++)
        {
            // BN offset is units of 2*384/32 = 24
            ymm0 = p_cnProcBuf   [j*24 + i];
            ymm1 = p_cnProcBufRes[j*24 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 10 BNs

    if (lut_numCnInCnGroups[7] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[7]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 10
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[7]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[7]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<10; j++)
            {
                // BN offset is units of 1*384/32 = 12
                ymm0 = p_cnProcBuf   [j*12 + i];
                ymm1 = p_cnProcBufRes[j*12 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<10; j++)
        {
            // BN offset is units of 1*384/32 = 12
            ymm0 = p_cnProcBuf   [j*12 + i];
            ymm1 = p_cnProcBufRes[j*12 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 19 BNs

    if (lut_numCnInCnGroups[8] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[8]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 19
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[8]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[8]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN (Last BN is connected to multiple CNs)
            // Compute PC for 32 CNs at once
            for (j=0; j<19; j++)
            {
                // BN offset is units of 4*384/32 = 48
                ymm0 = p_cnProcBuf   [j*48 + i];
                ymm1 = p_cnProcBufRes[j*48 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN (Last BN is connected to multiple CNs)
        // Compute PC for 32 CNs at once
        for (j=0; j<19; j++)
        {
            // BN offset is units of 4*384/32 = 48
            ymm0 = p_cnProcBuf   [j*48 + i];
            ymm1 = p_cnProcBufRes[j*48 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    return pcResSum;
#endif
}

/**
   \brief Performs parity check for BG2 on the CN processing buffer. Stops as soon as error is detected.
   \param p_lut Pointer to decoder LUTs
   \param Z Lifting size
   \return 32-bit parity check indicator
*/
static inline uint32_t nrLDPC_cnProcPc_BG2(t_nrLDPC_lut* p_lut, int8_t* cnProcBuf, int8_t* cnProcBufRes, uint16_t Z)
{
#if defined(__riscv) && defined(__riscv_vector)
    const uint8_t *lut_numCnInCnGroups = p_lut->numCnInCnGroups;
    const uint32_t *lut_startAddrCnGroups = p_lut->startAddrCnGroups;
    static const uint8_t numBN_per_group[6] = {3, 4, 5, 6, 8, 10};

    for (int grp = 0; grp < 6; grp++) {
        if (lut_numCnInCnGroups[grp] == 0)
            continue;
        uint32_t numBytes = (uint32_t)lut_numCnInCnGroups[grp] * Z;
        uint32_t off = lut_numCnInCnGroups_BG2_R15[grp] * NR_LDPC_ZMAX;
        if (nrLDPC_cnProcPc_group_rvv(&cnProcBuf[lut_startAddrCnGroups[grp]],
                                      &cnProcBufRes[lut_startAddrCnGroups[grp]],
                                      numBN_per_group[grp], numBytes, off))
            return 1;
    }
    return 0;
#else

    const uint8_t*  lut_numCnInCnGroups   = p_lut->numCnInCnGroups;
    const uint32_t* lut_startAddrCnGroups = p_lut->startAddrCnGroups;

    simde__m256i* p_cnProcBuf;
    simde__m256i* p_cnProcBufRes;

    // Number of CNs in Groups
    uint32_t M;
    uint32_t i;
    uint32_t j;
    uint32_t pcRes = 0;
    uint32_t pcResSum = 0;
    uint32_t Mrem;
    uint32_t M32;

    simde__m256i ymm0, ymm1;

    // =====================================================================
    // Process group with 3 BNs

    if (lut_numCnInCnGroups[0] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[0]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 3
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[0]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[0]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<3; j++)
            {
                // BN offset is units of (6*384/32) = 72
                ymm0 = p_cnProcBuf   [j*72 + i];
                ymm1 = p_cnProcBufRes[j*72 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<3; j++)
        {
            // BN offset is units of (6*384/32) = 72
            ymm0 = p_cnProcBuf   [j*72 + i];
            ymm1 = p_cnProcBufRes[j*72 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 4 BNs

    if (lut_numCnInCnGroups[1] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[1]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 4
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[1]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[1]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<4; j++)
            {
                // BN offset is units of 20*384/32 = 240
                ymm0 = p_cnProcBuf   [j*240 + i];
                ymm1 = p_cnProcBufRes[j*240 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<4; j++)
        {
            // BN offset is units of 20*384/32 = 240
            ymm0 = p_cnProcBuf   [j*240 + i];
            ymm1 = p_cnProcBufRes[j*240 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 5 BNs

    if (lut_numCnInCnGroups[2] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[2]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 5
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[2]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[2]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<5; j++)
            {
                // BN offset is units of 9*384/32 = 108
                ymm0 = p_cnProcBuf   [j*108 + i];
                ymm1 = p_cnProcBufRes[j*108 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<5; j++)
        {
            // BN offset is units of 9*384/32 = 108
            ymm0 = p_cnProcBuf   [j*108 + i];
            ymm1 = p_cnProcBufRes[j*108 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 6 BNs

    if (lut_numCnInCnGroups[3] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[3]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 6
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[3]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[3]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<6; j++)
            {
                // BN offset is units of 3*384/32 = 36
                ymm0 = p_cnProcBuf   [j*36 + i];
                ymm1 = p_cnProcBufRes[j*36 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<6; j++)
        {
            // BN offset is units of 3*384/32 = 36
            ymm0 = p_cnProcBuf   [j*36 + i];
            ymm1 = p_cnProcBufRes[j*36 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 8 BNs

    if (lut_numCnInCnGroups[4] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[4]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 8
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[4]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[4]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<8; j++)
            {
                // BN offset is units of 2*384/32 = 24
                ymm0 = p_cnProcBuf   [j*24 + i];
                ymm1 = p_cnProcBufRes[j*24 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<8; j++)
        {
            // BN offset is units of 2*384/32 = 24
            ymm0 = p_cnProcBuf   [j*24 + i];
            ymm1 = p_cnProcBufRes[j*24 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    // =====================================================================
    // Process group with 10 BNs

    if (lut_numCnInCnGroups[5] > 0)
    {
        // Reset results
        pcResSum = 0;

        // Number of CNs in group
        M = lut_numCnInCnGroups[5]*Z;
        // Remainder modulo 32
        Mrem = M&31;
        // Number of groups of 32 CNs for parallel processing
        // Ceil for values not divisible by 32
        M32 = (M + 31)>>5;

        // Set pointers to start of group 10
        p_cnProcBuf    = (simde__m256i*) &cnProcBuf   [lut_startAddrCnGroups[5]];
        p_cnProcBufRes = (simde__m256i*) &cnProcBufRes[lut_startAddrCnGroups[5]];

        // Loop over CNs
        for (i=0; i<(M32-1); i++)
        {
            pcRes = 0;
            // Loop over every BN
            // Compute PC for 32 CNs at once
            for (j=0; j<10; j++)
            {
                // BN offset is units of 2*384/32 = 24
                ymm0 = p_cnProcBuf   [j*24 + i];
                ymm1 = p_cnProcBufRes[j*24 + i];

                // Add BN and input LLR, extract the sign bit
                // and add in GF(2) (xor)
                pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
            }

            // If no error pcRes should be 0
            pcResSum |= pcRes;
        }

        // Last 32 CNs might not be full valid 32 depending on Z
        pcRes = 0;
        // Loop over every BN
        // Compute PC for 32 CNs at once
        for (j=0; j<10; j++)
        {
            // BN offset is units of 2*384/32 = 24
            ymm0 = p_cnProcBuf   [j*24 + i];
            ymm1 = p_cnProcBufRes[j*24 + i];

            // Add BN and input LLR, extract the sign bit
            // and add in GF(2) (xor)
            pcRes ^= simde_mm256_movemask_epi8(simde_mm256_adds_epi8(ymm0,ymm1));
        }

        // If no error pcRes should be 0
        // Only use valid CNs
        if (Mrem)
          pcResSum |= (pcRes&(0xFFFFFFFF>>(32-Mrem)));

        // If PC failed we can stop here
        if (pcResSum > 0)
        {
            return pcResSum;
        }
    }

    return pcResSum;
#endif
}

#endif



