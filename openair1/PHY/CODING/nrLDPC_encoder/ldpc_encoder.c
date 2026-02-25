/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the LDPC encoder
 */



#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "assertions.h"
#include "openair1/PHY/CODING/nrLDPC_defs.h"
#include "openair1/PHY/CODING/nrLDPC_extern.h"
#include "ldpc_generate_coefficient.c"

int LDPCencoder(unsigned char **inputArray, unsigned char *outputArray, encoder_implemparams_t *impp)
{
  const unsigned char *input = inputArray[0];
  // channel input is the output of this function!
  unsigned char *output = outputArray;
  const int Zc = impp->Zc;
  const int Kb = impp->Kb;
  const short block_length = impp->K;
  const short BG = impp->BG;
  const uint8_t gen_code = impp->gen_code;
  uint8_t c[22*384]; //padded input, unpacked, max size
  uint8_t d[68 * 384]; // coded output, unpacked, max size

  int i,i1,i2,i3,i4,i5,temp_prime,var;
  int no_punctured_columns, removed_bit;
  int nind=0;
  int indlist[1000];

  const short *Gen_shift_values = choose_generator_matrix(BG, Zc);
  if (Gen_shift_values==NULL) {
    printf("ldpc_encoder_orig: could not find generator matrix BG %d Zc %d\n",BG,Zc);
    return(-1);
  }

  //printf("ldpc_encoder_orig: BG %d, Zc %d, Kb %d\n",BG, Zc, Kb);

  AssertFatal(BG <= 2, "BG %d is not supported yet\n", BG);
  // load base graph of generator matrix
  const short nrows = BG == 1 ? 46 : 42;
  const short ncols = BG == 1 ? 22 : 10;
  const short rate = BG == 1 ? 3 : 5;
  const short *no_shift_values = BG == 1 ? no_shift_values_BG1 : no_shift_values_BG2;
  const short *pointer_shift_values = BG == 1 ? pointer_shift_values_BG1 : pointer_shift_values_BG2;

  no_punctured_columns=(int)((nrows-2)*Zc+block_length-block_length*rate)/Zc;
  removed_bit=(nrows-no_punctured_columns-2) * Zc+block_length-(block_length*rate);
  //printf("%d\n",no_punctured_columns);
  //printf("%d\n",removed_bit);
  // unpack input
  memset(c,0,sizeof(uint8_t) * ncols * Zc);
  memset(d,0,sizeof(uint8_t) * nrows * Zc);

  for (i=0; i<block_length; i++)
  {
    // c[i] = input[i/8]<<(i%8);
    // c[i]=c[i]>>7&1;
    c[i] = (input[i / 8] & (128 >> (i & 7))) >> (7 - (i & 7));
  }

  // parity check part

  if (gen_code==1 || gen_code==2 || gen_code==3)
  {
    char fname[100];
    sprintf(fname,"ldpc_BG%d_Zc%d_byte.c",BG,Zc);
    FILE *fd=fopen(fname,"w");
    AssertFatal(fd!=NULL,"cannot open %s\n",fname);


    int shift;
    char data_type[100];
    char xor_command[100];
    int mask;
    char permutex_command[100];
    int use_permutex=0;
    char alignr_command[100];
    int use_alignr=0;


    fprintf(fd,"#include \"PHY/sse_intrin.h\"\n");

    if (gen_code == 1 && (Zc&63)==0) {
      shift=6;
      mask=63;
      strcpy(data_type,"__m512i");
      strcpy(xor_command,"_mm512_xor_si512");
      strcpy(permutex_command,"_mm512_permutex2var_epi8");
      strcpy(alignr_command,"_mm512_alignr_epi8");
      use_alignr=1;
    }
    else if (gen_code == 1 && (Zc&31)==0) {
      shift=5; // AVX2 - 256-bit SIMD
      mask=31;
      strcpy(data_type,"simde__m256i");
      strcpy(xor_command,"simde_mm256_xor_si256");
    }
    else if ((gen_code <=2) && (Zc&15)==0) {
      shift=4; // SSE4 - 128-bit SIMD
      mask=15;
      strcpy(data_type,"simde__m128i");
      strcpy(xor_command,"simde_mm_xor_si128");
      strcpy(alignr_command,"simde_mm_alignr_epi8");
      use_alignr=1;
    }
    else if ((gen_code <=2) && (Zc&7)==0) {
      shift=3; // MMX  - 64-bit SIMD
      mask=7;
      strcpy(data_type,"simde__m64");
      strcpy(xor_command,"simde_mm_xor_si64"); 
    }
    else {
      shift=0;                 // no SIMD
      mask=0;
      strcpy(data_type,"uint8_t");
      strcpy(xor_command,"scalar_xor");
      fprintf(fd,"#define scalar_xor(a,b) ((a)^(b))\n");

    }
    fprintf(fd,"// generated code for Zc=%d, byte encoding\n",Zc);
    if (use_permutex==1) {
	 fprintf(fd,"  const uint8_t abshift_bytes[%d][%d] __attribute__((aligned(64)))= {\n",1+mask,1+mask); 
         for (int i=0;i<=mask;i++) {
	    fprintf(fd,"{");
	    for (int j=0;j<=mask;j++) {
              fprintf(fd,"%d",i+j);
	      if (j<mask) fprintf(fd,",");
	      else fprintf(fd,"}");
	    }
	    if (i<mask) fprintf(fd,",\n");
	    else fprintf(fd,"};\n");
      	}
	fprintf(fd,"static inline __m512i abshift_get(int r)\n {\n");
        fprintf(fd,"   return * (const __m512i *) abshift_bytes[r];\n}");
    }
    fprintf(fd,"static inline void ldpc_BG%d_Zc%d_byte(uint8_t *c,uint8_t *d) {\n",BG,Zc);
    fprintf(fd,"  %s *csimd=(%s *)c,*dsimd=(%s *)d;\n\n",data_type,data_type,data_type);
    fprintf(fd,"  %s *c2,*d2;\n\n",data_type);
    fprintf(fd,"  int i2;\n");
    fprintf(fd,"  for (i2=0; i2<%d; i2++) {\n",Zc>>shift);

    for (i2=0; i2 < 1; i2++)
    {
      //t=Kb*Zc+i2;
    
      // calculate each row in base graph
     

      fprintf(fd,"     c2=&csimd[i2];\n");
      fprintf(fd,"     d2=&dsimd[i2];\n");

      for (i1 = 0; i1 < nrows; i1++)
      {
        fprintf(fd,"\n//row: %d\n",i1);
        AssertFatal(shift >= 0 , "The result of the right shift is undefined because the right operand is negative\n");
	fprintf(fd,"     d2[%d]=",(Zc*i1)>>shift);

        nind=0;

        for (i3=0; i3 < ncols; i3++)
        {
          temp_prime=i1 * ncols + i3;
	  for (i4=0; i4 < no_shift_values[temp_prime]; i4++)
  	  {
	      var=(int)((i3*Zc + (Gen_shift_values[ pointer_shift_values[temp_prime]+i4 ]+1)%Zc)/Zc);
	      int index =var*2*Zc + (i3*Zc + (Gen_shift_values[ pointer_shift_values[temp_prime]+i4 ]+1)%Zc) % Zc;
	      printf("var %d, i3 %d, i4 %d, index %d, shift %d, Zc %d, pointer_shift_values[%d] %d gen_shift_value %d\n",var,i3,i4,index,shift,Zc,temp_prime,pointer_shift_values[temp_prime],Gen_shift_values[pointer_shift_values[temp_prime]]);
   	      printf("indlist[%d] %d, index&mask %d, index>>shift %d\n",nind,indlist[nind],index&mask,index>>shift);
	      if (use_permutex==1 || use_alignr==1) {
                indlist[nind++] = index;
	      }
	      else {
	        indlist[nind] = ((index&mask)*((2*Zc*ncols)>>shift)/* *Kb */)+(index>>shift);
	      }
	  }//i4
        }//i3
	for (i4=0;i4<nind-1;i4++) {
           if (use_permutex==0 && use_alignr==0) {
  	      fprintf(fd,"%s(c2[%d],",xor_command,indlist[i4]);
	   }
	   else {
              if ((indlist[i4]&mask) == 0) {
  	        fprintf(fd,"%s(c2[%d],",xor_command,indlist[i4]>>shift);
	      }
	      else if (use_permutex==1) {
                fprintf(fd,"%s(%s(c2[%d],abshift_get(%d),c2[%d]),",xor_command,permutex_command,(indlist[i4]>>shift),indlist[i4]&mask,(indlist[i4]>>shift)+1);
	      }
	      else if (use_alignr==1) {
                fprintf(fd,"%s(%s(c2[%d],c2[%d],%d),",xor_command,alignr_command,(indlist[i4]>>shift)+1,(indlist[i4]>>shift),indlist[i4]&mask);
	      }
	   }
	} //i4
	if (use_permutex==0 && use_alignr==0)
	  fprintf(fd,"c2[%d]",indlist[i4]);
	else {
          if ((indlist[i4]&mask) == 0) {
  	     fprintf(fd,"c2[%d]",indlist[i4]>>shift);
	  }
	  else if (use_permutex==1){
             fprintf(fd,"%s(c2[%d],abshift_get(%d),c2[%d])",permutex_command,(indlist[i4]>>shift),indlist[i4]&mask,(indlist[i4]>>shift)+1);
	  }
	  else if (use_alignr==1) {
             fprintf(fd,"%s(c2[%d],c2[%d],%d)",alignr_command,(indlist[i4]>>shift)+1,(indlist[i4]>>shift),indlist[i4]&mask);

	  }
	}
	for (i4=0;i4<nind-1;i4++) fprintf(fd,")"); 
  	fprintf(fd,";\n");
       } // i1 
       fprintf(fd,"  }\n}\n");
    } // i2
    fclose(fd);
  }
  else if (gen_code == 4) { // CUDA
    char fname[100];
    sprintf(fname,"ldpc_BG%d_Zc%d_32bit.cu",BG,Zc);
    FILE *fd=fopen(fname,"w");
    AssertFatal(fd!=NULL,"cannot open %s\n",fname);
    printf("Writing to %s\n",fname);
    fprintf(fd,"#include <stdio.h>\n#include <stdint.h>\n#include <cuda_runtime.h>\n");

    fprintf(fd,"// generated code for Zc=%d, byte encoding\n",Zc);
    fprintf(fd,"__global__ void ldpc_BG%d_Zc%d_worker(uint32_t *c[4],uint32_t *d[4]) {\n",BG,Zc);
    fprintf(fd,"  uint32_t *c32=c[blockIdx.x];\n  uint32_t *d32=d[blockIdx.x];\n\n");
    fprintf(fd,"  int i2 = threadIdx.x;\n");
    fprintf(fd,"  int i1 = blockIdx.y;\n");
    fprintf(fd,"  if (i2 < %d) {\n",Zc);
    fprintf(fd,"    c32+=i2;\n");
    fprintf(fd,"    d32+=i2;\n");
    fprintf(fd,"    switch(i1) {\n");

    for (int i1=0;i1<nrows;i1++) {
      nind = 0;
      fprintf(fd,"    case %d:\n",i1);
      fprintf(fd,"      d32[%d]=",(Zc*i1));
      for (i3=0; i3 < ncols; i3++)
      {
          temp_prime=i1 * ncols + i3;
	  for (i4=0; i4 < no_shift_values[temp_prime]; i4++)
  	  {
	     var=(int)((i3*Zc + (Gen_shift_values[ pointer_shift_values[temp_prime]+i4 ]+1)%Zc)/Zc);
  	     int index =var*2*Zc + (i3*Zc + (Gen_shift_values[ pointer_shift_values[temp_prime]+i4 ]+1)%Zc) % Zc;
	     printf("var %d, i3 %d, i4 %d, index %d (index mod 2Zc) %d, Zc %d, pointer_shift_values[%d] %d gen_shift_value %d offset %d\n",var,i3,i4,index,index%(2*Zc),Zc,temp_prime,pointer_shift_values[temp_prime],Gen_shift_values[pointer_shift_values[temp_prime]],(i3*Zc + (Gen_shift_values[ pointer_shift_values[temp_prime]+i4 ]+1)%Zc) % Zc);
	     if (index%(2*Zc) >= Zc) printf("***********************\n");
   	     indlist[nind] = index;
	     printf("indlist[%d] %d, index %d\n",nind,indlist[nind],index);
	     nind++;
   	  } //i4
      } // i3
      for (i4=0;i4<nind-1;i4++) {
         fprintf(fd,"c32[%d]^",indlist[i4]);
      } //i4
      fprintf(fd,"c32[%d];\n\n",indlist[i4]);
      fprintf(fd,"       break;\n");
    }// i1
    fprintf(fd,"     }\n");
    fprintf(fd,"  }\n");
    fprintf(fd,"}\n");

    fprintf(fd,"extern \"C\" int ldpc_BG%d_Zc%d_cuda32(uint32_t *c[4],uint32_t *d[4],int n_inputs) { \n",BG,Zc);
    fprintf(fd,"dim3 numblocks(n_inputs,%d);\n",nrows);
    fprintf(fd,"ldpc_BG%d_Zc%d_worker<<<numblocks,%d>>>(c,d);\n",BG,Zc,Zc);
    fprintf(fd," cudaDeviceSynchronize();\n");
    fprintf(fd,"  return(0);\n");
    fprintf(fd,"}\n");
    fclose(fd);
  }
  else if(gen_code==0)
  {
    for (i2=0; i2 < Zc; i2++)
    {
      //t=Kb*Zc+i2;

      //rotate matrix here
      for (i5=0; i5 < Kb; i5++)
      {
        const int temp = c[i5 * Zc];
        memmove(&c[i5 * Zc], &c[i5 * Zc + 1], Zc - 1);
        c[i5 * Zc + Zc - 1] = temp;
      }

      // calculate each row in base graph
      for (i1=0; i1 < nrows-no_punctured_columns; i1++)
      {
        unsigned char channel_temp = 0;

        for (i3 = 0; i3 < Kb; i3++) {
          temp_prime = i1 * ncols + i3;

          for (i4 = 0; i4 < no_shift_values[temp_prime]; i4++) {
            channel_temp = channel_temp ^ c[i3 * Zc + Gen_shift_values[pointer_shift_values[temp_prime] + i4]];
          }
        }

        d[i2+i1*Zc]=channel_temp;
        // output[t+i1*Zc]=channel_temp;
      }
    }
  }

  // information part and puncture columns
  memcpy(&output[0], &c[2 * Zc], block_length - 2 * Zc);
  memcpy(&output[block_length - 2 * Zc], &d[0], (nrows - no_punctured_columns) * Zc - removed_bit);
  // memcpy(output,c,Kb*Zc*sizeof(unsigned char));
  return block_length - 2 * Zc + (nrows - no_punctured_columns) * Zc - removed_bit;
}
