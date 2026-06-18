// deinterleave_u16.cu
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdint.h>

__global__ void deinterleave_i16_2(int16_t* __restrict__ e,
                                   const int16_t* __restrict__ f,
                                   int E1,
				   int E2,
				   int r_firstE2)
{
    int g = (int)(blockIdx.x * blockDim.x + threadIdx.x);
    int r = (int)blockIdx.y;
    int E = (r<r_firstE2) ? E1 : E2;
    int EQm = E/2;
    if (g >= EQm) return;

    int r_off = r<r_firstE2 ? r*E1 : ((r_firstE2*E1)+(r-r_firstE2)*E2);
    const int16_t *in = f +r_off + 2*g;

    int16_t* e0 = e + r_off;
    int16_t* e1 = e0 + EQm;
    e0[g] = in[0];
    e1[g] = in[1];
}

__global__ void deinterleave_i16_4(int16_t* __restrict__ e,
                                   const int16_t* __restrict__ f,
                                   int E1,
                       		   int E2,
				   int r_firstE2)
{
    int g = (int)(blockIdx.x * blockDim.x + threadIdx.x);
    int r = (int)blockIdx.y;
    int E = (r<r_firstE2) ? E1 : E2;
    int EQm = E/4;
    if (g >= EQm) return;

    int r_off = r<r_firstE2 ? r*E1 : ((r_firstE2*E1)+(r-r_firstE2)*E2);
    const int16_t *in = f + r_off + 4*g;
    int16_t* e0 = e + r_off;
    int16_t* e1 = e0 + EQm;
    int16_t* e2 = e1 + EQm;
    int16_t* e3 = e2 + EQm;
    e0[g] = in[0];
    e1[g] = in[1];
    e2[g] = in[2];
    e3[g] = in[3];
}

__global__ void deinterleave_i16_6(int16_t* __restrict__ e,
                                   const int16_t* __restrict__ f,
                                   int E1,
				   int E2,
				   int r_firstE2)
{
    int g = (int)(blockIdx.x * blockDim.x + threadIdx.x);
    int r = (int)blockIdx.y;
    int E = (r<r_firstE2) ? E1 : E2;
    int EQm = E/6;
    if (g >= EQm) return;

    int r_off = r<r_firstE2 ? r*E1 : (r_firstE2*E1)+(r-r_firstE2)*E2;
    const int16_t* in = f + r_off + 6*g;
    int16_t* e0 = e + r_off;
    int16_t* e1 = e0 + EQm;
    int16_t* e2 = e1 + EQm;
    int16_t* e3 = e2 + EQm;
    int16_t* e4 = e3 + EQm;
    int16_t* e5 = e4 + EQm;
    e0[g] = in[0];
    e1[g] = in[1];
    e2[g] = in[2];
    e3[g] = in[3];
    e4[g] = in[4];
    e5[g] = in[5];
}

__global__ void deinterleave_i16_8(int16_t* __restrict__ e,
                                   const int16_t* __restrict__ f,
                                   const int E1,
				   const int E2,
				   const int r_firstE2)
{
    int g = (int)(blockIdx.x * blockDim.x + threadIdx.x);
    int r = (int)blockIdx.y;
    int E = (r<r_firstE2) ? E1 : E2;
    int EQm = E/8;
    if (g >= EQm) return;
    int r_off = r<r_firstE2 ? r*E1 : (r_firstE2*E1)+(r-r_firstE2)*E2;

    const int16_t *in = (f + r_off + 8*g);
    int16_t* e0 = e + r_off;
    int16_t* e1 = e0 + EQm;
    int16_t* e2 = e1 + EQm;
    int16_t* e3 = e2 + EQm;
    int16_t* e4 = e3 + EQm;
    int16_t* e5 = e4 + EQm;
    int16_t* e6 = e5 + EQm;
    int16_t* e7 = e6 + EQm;
    e0[g] = in[0];
    e1[g] = in[1];
    e2[g] = in[2];
    e3[g] = in[3];
    e4[g] = in[4];
    e5[g] = in[5];
    e6[g] = in[6];
    e7[g] = in[7];
}

// Host launcher
extern "C" void launch_deinterleave_i16(int Qm, int E1, int E2, int C, int r_firstE2,int16_t* e, const int16_t* f,cudaStream_t *s,int8_t sidx)
{
    const int threads = 256;
    dim3 blocks(((E2/Qm)  + threads - 1) / threads,C);

    switch (Qm) {
        case 2: deinterleave_i16_2<<<blocks, threads, 0, s[sidx]>>>(e, f, E1,E2,r_firstE2); break;
        case 4: deinterleave_i16_4<<<blocks, threads, 0, s[sidx]>>>(e, f, E1,E2,r_firstE2); break;
        case 6: deinterleave_i16_6<<<blocks, threads, 0, s[sidx]>>>(e, f, E1,E2,r_firstE2); break;
        case 8: deinterleave_i16_8<<<blocks, threads, 0, s[sidx]>>>(e, f, E1,E2,r_firstE2); break;
        default: /* unsupported */ break;
    }
    cudaError_t err=cudaPeekAtLastError();
  
    if (err!=cudaSuccess) {
      printf("cuda error (deinterleave_i16): %s (e %p, f %p, E1 %d, E2 %d, Qm %d, C %d)\n",cudaGetErrorString(err),e,f,E1,E2,Qm,C);
      exit(-1);
    }

}
