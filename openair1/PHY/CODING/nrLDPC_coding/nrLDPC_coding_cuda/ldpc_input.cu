#include <stdio.h>
#include <stdint.h>
#include <cuda_runtime.h>
/*
__device__ const uint32_t masks[4] = {0x80,0x8000,0x800000,0x80000000};
__global__ void ldpc_input_worker(uint32_t **input,uint32_t *cc[4],int block_length,int nseg) {

  int block_off = blockIdx.x*blockDim.x<<2;	
  int i2 = threadIdx.x<<2;
  uint32_t *out=cc[blockIdx.y] + block_off + i2;
  int nseg0 = (blockIdx.y << 5);
  int nseg1;
  if ((nseg0 + 32) <= nseg) nseg1 = nseg0+32;
  else nseg1 = nseg0 + (nseg&31);
  int bit_offset = i2+block_off;
  int uint32_offset = bit_offset>>5;  
  uint32_t mask = masks[(bit_offset&31)>>3]; 
  uint32_t mask0 = mask>>(bit_offset&7);bit_offset++;
  uint32_t mask1 = mask>>(bit_offset&7);bit_offset++;
  uint32_t mask2 = mask>>(bit_offset&7);bit_offset++;
  uint32_t mask3 = mask>>(bit_offset&7);
  uint32_t tmp,jmod;
  uint32_t otmp0,otmp1,otmp2,otmp3;
  if (bit_offset  < block_length) {
      tmp=input[nseg0][uint32_offset];
      otmp0 = ((tmp&mask0) > 0); 
      otmp1 = ((tmp&mask1) > 0); 
      otmp2 = ((tmp&mask2) > 0); 
      otmp3 = ((tmp&mask3) > 0); 
      for (int j=nseg0+1;j<nseg1;j++) {
	 tmp=input[j][uint32_offset];
	 jmod = j&31;
         otmp0 |= (((tmp&mask0) > 0)<<jmod); 
         otmp1 |= (((tmp&mask1) > 0)<<jmod); 
         otmp2 |= (((tmp&mask2) > 0)<<jmod); 
         otmp3 |= (((tmp&mask3) > 0)<<jmod); 
      }	      
      out[0]=otmp0;
      out[1]=otmp1;
      out[2]=otmp2;
      out[3]=otmp3;
  } 
}
*/

__device__ uint32_t masks[32] = {
	0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1,
	0x8000,0x4000,0x2000,0x1000,0x800,0x400,0x200,0x100,
	0x800000,0x400000,0x200000,0x100000,0x80000,0x40000,0x20000,0x10000,
	0x80000000,0x40000000,0x20000000,0x10000000,0x8000000,0x4000000,0x2000000,0x1000000};
__global__ void ldpc_input_worker(uint32_t **input,uint32_t *cc[4],int nseg) {

//  int block_off = blockIdx.y*blockDim.x;	
  int i1 = blockIdx.y;	
  int i2 = threadIdx.x;
  int nseg0 = (blockIdx.x << 5);
  int nseg1;
  if ((nseg0 + 32) <= nseg) nseg1 = nseg0+32;
  else nseg1 = nseg0 + (nseg&31);
  int bit_offset = i2+(i1*384);
  int uint32_offset = bit_offset>>5;  
  uint32_t mask0 = masks[bit_offset&31];
  uint32_t tmp,jmod;
  uint32_t otmp0;
  if (bit_offset  < 8448) {
      tmp=input[nseg0][uint32_offset];
      otmp0 = ((tmp&mask0) > 0); 
      for (int j=nseg0+1;j<nseg1;j++) {
	 tmp=input[j][uint32_offset];
	 jmod = j&31;
         otmp0 |= (((tmp&mask0) > 0)<<jmod); 
      }	      
      
      cc[blockIdx.x][(2*i1*384) + i2] = otmp0;
      cc[blockIdx.x][(2*i1+1)*384 + i2]=otmp0;
  } 
}
extern "C" int ldpc_input(uint32_t **input,uint32_t *cc[4],int nseg,cudaStream_t *stream,int sidx) { 

 int ns = nseg>>5;
 if ((nseg&31)>0) ns++;

 dim3 numblocks(ns,22);
 //printf("input %p\n",input);
 ldpc_input_worker<<<numblocks,384,0,stream[sidx]>>>(input,cc,nseg);
 cudaError_t err=cudaPeekAtLastError();
 if (err!=cudaSuccess) {
    printf("cuda error: %s (input %p, cc %p, nseg %d, ns %d)\n",cudaGetErrorString(err),input,cc,nseg,ns);
    exit(-1);
 }
 return(0);
}
