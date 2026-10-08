/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the CUDA version of nrLDPC decoder, including initialization and driver warmup mechanisms.
 * \note Optimized for NVIDIA GH200 (Grace Hopper) architecture using Zero-Copy access.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // pthread_setname_np()
#endif
#include <stdint.h>
#include "PHY/sse_intrin.h"
#include "log.h"
#include "openair1/PHY/CODING/coding_defs.h"

#include "openair1/PHY/CODING/nrLDPC_extern.h"

#ifdef NR_LDPC_DEBUG_MODE
#include "nrLDPC_tools/nrLDPC_debug.h"
#endif

// decoder interface
/**
   \brief LDPC decoder API type definition
   \param p_decParams LDPC decoder parameters
   \param p_llr Input LLRs
   \param p_llrOut Output vector
   \param p_profiler LDPC profiler statistics
*/

//--------------------------CUDA Area---------------------------
#include <cuda_runtime.h>
#include "nrLDPC_CUDA_shared_param.h"
#include "nrLDPC_coding_cuda_config.h"
#include "nrLDPC_coding_cuda_ctx.h"

extern cudaStream_t decoderStreams[MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4];
static bool decoder_streamsCreated = false;
static volatile int cuda_graph_breaker = 0;// 0 by default to enable the CUDA graph
cudaError_t Err;

int8_t* cnProcBuf_dev;
int8_t* bnProcBuf_dev;
int8_t* llrRes_dev;
int8_t* llrProcBuf_dev;

int8_t* p_llr_dev;
int8_t* p_out_dev;

extern int pageable, integrated, pageable_uses_host;

int cuda_support_init_decoder()
{
  // use cudaMalloc for all inner buffers
  cudaError_t err;

  err = cudaMalloc((void**)&cnProcBuf_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_SIZE_CN_PROC_BUF);
  AssertFatal(err == cudaSuccess, "CUDA Error (cnProcBuf_dev): %s\n", cudaGetErrorString(err));

  err = cudaMalloc((void**)&bnProcBuf_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_SIZE_BN_PROC_BUF);
  AssertFatal(err == cudaSuccess, "CUDA Error (bnProcBuf_dev): %s\n", cudaGetErrorString(err));

  err = cudaMalloc((void**)&llrRes_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_MAX_NUM_LLR);
  AssertFatal(err == cudaSuccess, "CUDA Error (llrRes_dev): %s\n", cudaGetErrorString(err));

  err = cudaMalloc((void**)&llrProcBuf_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_MAX_NUM_LLR);
  AssertFatal(err == cudaSuccess, "CUDA Error (llrProcBuf_dev): %s\n", cudaGetErrorString(err));

  err = cudaMalloc((void**)&p_llr_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_MAX_NUM_LLR);
  AssertFatal(err == cudaSuccess, "CUDA Error (p_llr_dev): %s\n", cudaGetErrorString(err));
  cudaMemset(p_llr_dev, 0, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_MAX_NUM_LLR);
  err = cudaMalloc((void**)&p_out_dev, sizeof(int8_t) * MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4 * NR_LDPC_MAX_NUM_LLR);
  AssertFatal(err == cudaSuccess, "CUDA Error (p_llr_dev): %s\n", cudaGetErrorString(err));

  printf("[CUDA] Intermediate buffers allocated in Device Memory.\n");

  return 0;
}

static ldpc_cuda_bridge_t* stream_bridges[8];

extern cudaError_t nrLDPC_decoder_cuda_GraphRecord(ldpc_cuda_bridge_t* buffer,
                                                   uint32_t numLLR,
                                                   int8_t* cnProcBuf,
                                                   int8_t* bnProcBuf,
                                                   int8_t* llrRes,
                                                   int8_t* llrProcBuf,
                                                   uint32_t Z,
                                                   uint32_t K,
                                                   uint8_t BG,
                                                   uint8_t R,
                                                   uint8_t numMaxIter,
                                                   uint8_t n_segments,
                                                   e_nrLDPC_outMode outMode,
                                                   cudaStream_t* streams,
                                                   uint8_t CudaStreamIdx,
                                                   cudaGraph_t* graphPtr,
                                                   cudaGraphExec_t* graphExecPtr,
                                                   uint8_t* isCreatedFlag);

extern cudaError_t nrLDPC_decoder_cuda_GraphExecute(cudaGraphExec_t graphExec,
                                                    cudaStream_t stream,
                                                    cudaEvent_t* doneEvent,
                                                    uint8_t CudaStreamIdx);

extern void nrLDPC_decoder_cuda_NormalExecute(ldpc_cuda_bridge_t* buffer,
                                              uint32_t numLLR,
                                              int8_t* cnProcBuf,
                                              int8_t* bnProcBuf,
                                              int8_t* llrRes,
                                              int8_t* llrProcBuf,
                                              uint32_t Z,
                                              uint32_t K,
                                              uint8_t BG,
                                              uint8_t R,
                                              uint8_t numMaxIter,
                                              uint8_t n_segments,
                                              e_nrLDPC_outMode outMode,
                                              cudaStream_t* streams,
                                              uint8_t CudaStreamIdx,
                                              cudaEvent_t* doneEvent);

static inline uint32_t nrLDPC_decoder_core_dynamic(int8_t* p_llr,
                                                   int8_t* p_out,
                                                   int n_segments,
                                                   t_nrLDPC_dec_params* p_decParams,
                                                   t_nrLDPC_time_stats* p_profiler,
                                                   decode_abort_t* ab);
#define MAX_GRAPH_CACHE_SIZE 16
#define PRE_RECORDED_COUNT 6
#define STATIC_SEG_SIZE 9 // n_segments in pre-record graphs, should be determined for real cases

typedef struct {
  uint32_t Z;
  uint32_t K;
  uint32_t numLLR;
  uint8_t R;
  uint8_t BG;
  uint8_t numMaxIter;
  uint16_t n_segments;
  e_nrLDPC_outMode outMode;
  cudaGraph_t graph;
  cudaGraphExec_t exec;
  ldpc_cuda_bridge_t* bridge_ptr;
  bool occupied;
} gpu_graph_node_t;

static gpu_graph_node_t gpu_graph_cache[MAX_GRAPH_CACHE_SIZE];
static int dynamic_cache_idx = PRE_RECORDED_COUNT;

void init_decoder_warmup()
{
  // =====================================================================
  // CUDA Driver Warm-up
  // Purpose: Execute a few representative graphs to trigger CUDA context
  // initialization and driver-level JIT/lazy loading.
  // Note: These specific Z/R combinations might not match the actual
  // run-time traffic, but running them ensures the GPU pipeline is ready.
  // =====================================================================

  cudaError_t err_warmup = cudaSuccess;

  // Sample configurations for warmup
  uint32_t Z_list[] = {320, 352, 384};
  uint8_t R_list[] = {13, 23};
  int node_idx = 0;

  int8_t* dummy_input_llr = NULL;
  int8_t* dummy_output_bits = NULL;
  uint32_t max_z = 384;
  uint32_t max_n_segs = STATIC_SEG_SIZE;

  size_t input_size_bytes = 68 * max_z * max_n_segs * sizeof(int8_t);
  size_t output_size_bytes = 8448 * max_n_segs * sizeof(int8_t);

  // Allocate Pinned/Mapped Memory for Zero-Copy access (GH200 friendly)
  cudaError_t err = cudaHostAlloc((void**)&dummy_input_llr, input_size_bytes, cudaHostAllocMapped);
  AssertFatal(err == cudaSuccess, "cudaHostAlloc() dummy_input_llr: %s\n", cudaGetErrorString(err));
  err = cudaHostAlloc((void**)&dummy_output_bits, output_size_bytes, cudaHostAllocMapped);
  AssertFatal(err == cudaSuccess, "cudaHostAlloc() dummy_output_bits: %s\n", cudaGetErrorString(err));

  memset(dummy_input_llr, 0, input_size_bytes);
  memset(dummy_output_bits, 0, output_size_bytes);

  printf("[CUDA] Initializing & Warming up Driver Pipeline...\n");
  if (cuda_graph_breaker == 0) {
    for (int r_idx = 0; r_idx < 2; r_idx++) {
      for (int z_idx = 0; z_idx < 3; z_idx++) {
        uint32_t Z = Z_list[z_idx];
        uint8_t R = R_list[r_idx];
        uint8_t BG = 1;
        uint32_t K = 22 * Z;
        uint32_t numLLR = (R == 13) ? NR_LDPC_NCOL_BG1_R13 * Z : NR_LDPC_NCOL_BG1_R23 * Z;
        uint8_t numMaxIter = 2; // 2 iterations for faster warmup
        uint8_t n_segments = STATIC_SEG_SIZE;

        // Bind dummy buffers
        gpu_graph_cache[node_idx].bridge_ptr->p_llr_ptr = dummy_input_llr;
        gpu_graph_cache[node_idx].bridge_ptr->p_out_ptr = dummy_output_bits;

        // Record graph
        err_warmup = nrLDPC_decoder_cuda_GraphRecord(gpu_graph_cache[node_idx].bridge_ptr,
                                                     numLLR,
                                                     cnProcBuf_dev,
                                                     bnProcBuf_dev,
                                                     llrRes_dev,
                                                     llrProcBuf_dev,
                                                     Z,
                                                     K,
                                                     BG,
                                                     R,
                                                     numMaxIter,
                                                     n_segments,
                                                     nrLDPC_outMode_BIT,
                                                     decoderStreams,
                                                     0,
                                                     &gpu_graph_cache[node_idx].graph,
                                                     &gpu_graph_cache[node_idx].exec,
                                                     (uint8_t*)&gpu_graph_cache[node_idx].occupied);
        if (err_warmup != cudaSuccess) {
          cuda_graph_breaker = 1; // Once a graph recording fails, forbidden the graph recording permanently
          printf("[CUDA] Warmup Graph Record Failed (err=%s). Circuit breaker triggered\n", cudaGetErrorString(err_warmup));
          break;
        }

        // Save metadata
        gpu_graph_cache[node_idx].Z = Z;
        gpu_graph_cache[node_idx].R = R;
        gpu_graph_cache[node_idx].K = K;
        gpu_graph_cache[node_idx].numLLR = numLLR;
        gpu_graph_cache[node_idx].BG = BG;
        gpu_graph_cache[node_idx].numMaxIter = numMaxIter;
        gpu_graph_cache[node_idx].n_segments = n_segments;
        gpu_graph_cache[node_idx].outMode = nrLDPC_outMode_BIT;

        node_idx++;
      }
    }
    dynamic_cache_idx = node_idx;

    // Execute to trigger driver initialization
    if (dynamic_cache_idx > 0) {
      for (int i = 0; i < dynamic_cache_idx; i++) {
        if (gpu_graph_cache[i].occupied) {
          nrLDPC_decoder_cuda_GraphExecute(gpu_graph_cache[i].exec, decoderStreams[0], NULL, 0);
        }
      }
      cudaDeviceSynchronize();
      printf("[CUDA] Driver warm-up complete. Executed %d dummy graphs.\n", dynamic_cache_idx);

    for (int i = 0; i < dynamic_cache_idx; i++) {
        if (gpu_graph_cache[i].occupied) {
            if (gpu_graph_cache[i].exec) {
                cudaGraphExecDestroy(gpu_graph_cache[i].exec);
                gpu_graph_cache[i].exec = NULL;
            }
            if (gpu_graph_cache[i].graph) {
                cudaGraphDestroy(gpu_graph_cache[i].graph);
                gpu_graph_cache[i].graph = NULL;
            }
            
            if (gpu_graph_cache[i].bridge_ptr) {
                gpu_graph_cache[i].bridge_ptr->p_llr_ptr = NULL;
                gpu_graph_cache[i].bridge_ptr->p_out_ptr = NULL;
            }
            
            gpu_graph_cache[i].occupied = false;
        }
    }

      // Mark slots as free and reset index so real traffic starts from slot 0
      for (int i = 0; i < dynamic_cache_idx; i++) {
        gpu_graph_cache[i].occupied = false;
      }
      printf("[CUDA] Cache cleared. Ready for dynamic recording.\n");
    }
  }
  dynamic_cache_idx = 0;

  if (cuda_graph_breaker == 1) {
    printf("[CUDA] Using Normal Execution for Warmup (Graph Disabled).\n");
    for (int r_idx = 0; r_idx < 2; r_idx++) {
      for (int z_idx = 0; z_idx < 3; z_idx++) {
        uint32_t Z = Z_list[z_idx];
        uint8_t R = R_list[r_idx];
        uint8_t BG = 1;
        uint32_t K = 22 * Z;
        uint32_t numLLR = (R == 13) ? NR_LDPC_NCOL_BG1_R13 * Z : NR_LDPC_NCOL_BG1_R23 * Z;
        uint8_t numMaxIter = 2; // 2 iterations for faster warmup
        uint8_t n_segments = STATIC_SEG_SIZE;

        // Bind dummy buffers
        gpu_graph_cache[0].bridge_ptr->p_llr_ptr = dummy_input_llr;
        gpu_graph_cache[0].bridge_ptr->p_out_ptr = dummy_output_bits;

        // normal execution
        nrLDPC_decoder_cuda_NormalExecute(gpu_graph_cache[0].bridge_ptr,
                                          numLLR,
                                          cnProcBuf_dev,
                                          bnProcBuf_dev,
                                          llrRes_dev,
                                          llrProcBuf_dev,
                                          Z,
                                          K,
                                          BG,
                                          R,
                                          numMaxIter,
                                          n_segments,
                                          nrLDPC_outMode_BIT,
                                          decoderStreams,
                                          0,
                                          NULL);
      }
    }
    cudaDeviceSynchronize();
    printf("[CUDA] Driver warm-up complete with normal execution\n");
  }
  cudaFreeHost(dummy_input_llr);
  cudaFreeHost(dummy_output_bits);
}
void init_decoder_gpu_structures()
{
  printf("[CUDA] Initializing Global GPU Structures...\n");
  // Bridge for graphs
  for (int i = 0; i < MAX_GRAPH_CACHE_SIZE; i++) {
    if (gpu_graph_cache[i].bridge_ptr == NULL) {
      cudaError_t err  = cudaHostAlloc((void**)&gpu_graph_cache[i].bridge_ptr, sizeof(ldpc_cuda_bridge_t), cudaHostAllocMapped);
      AssertFatal(err == cudaSuccess, "cudaHostAlloc() gpu_graph_cache: %s\n", cudaGetErrorString(err));

      gpu_graph_cache[i].bridge_ptr->p_llr_ptr = NULL;
      gpu_graph_cache[i].bridge_ptr->p_out_ptr = NULL;
      gpu_graph_cache[i].occupied = false;
    }
  }
  printf("[CUDA] Allocated %d Graph Bridges.\n", MAX_GRAPH_CACHE_SIZE);
  // Bridge for normal execute
  for (int i = 0; i < 8; i++) {
    if (stream_bridges[i] == NULL) {
      cudaError_t err = cudaHostAlloc((void**)&stream_bridges[i], sizeof(ldpc_cuda_bridge_t), cudaHostAllocMapped);
      AssertFatal(err == cudaSuccess, "cudaHostAlloc() stream_bridges: %s\n", cudaGetErrorString(err));
      stream_bridges[i]->p_llr_ptr = NULL;
      stream_bridges[i]->p_out_ptr = NULL;
    }
  }
  printf("[CUDA] Allocated %d Stream Bridges for Fallback case.\n", 8);
}

void init_decoder_graphs()
{
  for (int i = 0; i < MAX_GRAPH_CACHE_SIZE; i++) {
    gpu_graph_cache[i].occupied = false;
    gpu_graph_cache[i].graph = NULL;
    gpu_graph_cache[i].exec = NULL;
    gpu_graph_cache[i].bridge_ptr = NULL;
    gpu_graph_cache[i].Z = 0;
    gpu_graph_cache[i].R = 0;
  }

  dynamic_cache_idx = 0;

  printf("[decoder_graphs] initialized %d dynamic cache slots\n", MAX_GRAPH_CACHE_SIZE);
}

void free_graphs()
{
  for (int i = 0; i < MAX_GRAPH_CACHE_SIZE; i++) {
    if (gpu_graph_cache[i].occupied) {
      if (gpu_graph_cache[i].exec)
        cudaGraphExecDestroy(gpu_graph_cache[i].exec);
      if (gpu_graph_cache[i].graph)
        cudaGraphDestroy(gpu_graph_cache[i].graph);
      gpu_graph_cache[i].occupied = false;
    }
  }
  printf("[decoder_graphs] shutdown complete (Dynamic Cache Cleared)\n");
}

extern int cuda_support_set;

bool encoder_streamsCreated = false;
cudaStream_t encoderStreams[4];
void cuda_support_init();

int32_t LDPCinit_cuda()
{
  if (cuda_support_set == 0) {
    printf("Calling encoder initializations\n");
    cuda_support_init();
  }
  if (!decoder_streamsCreated) {
    for (int s = 0; s < 8; ++s) {
      cudaStreamCreateWithFlags(&decoderStreams[s], cudaStreamNonBlocking);
    }
    decoder_streamsCreated = true;
  }

  if (!encoder_streamsCreated) {
    for (int s = 0; s < 4; ++s) {
      cudaStreamCreateWithFlags(&encoderStreams[s], cudaStreamNonBlocking);
    }
    encoder_streamsCreated = true;
  }
  printf("CUDA LDPC decoder initiating\n");
  cuda_support_init_decoder();
  init_decoder_graphs();
  init_decoder_gpu_structures();
  init_decoder_warmup();
  return 0;
}

int32_t LDPCshutdown_cuda()
{
  if (cnProcBuf_dev) { cudaFree(cnProcBuf_dev); cnProcBuf_dev = NULL; }
  if (bnProcBuf_dev) { cudaFree(bnProcBuf_dev); bnProcBuf_dev = NULL; }
  if (llrRes_dev)   { cudaFree(llrRes_dev);   llrRes_dev = NULL; }
  if (llrProcBuf_dev) { cudaFree(llrProcBuf_dev); llrProcBuf_dev = NULL; }

  if (p_llr_dev)   { cudaFree(p_llr_dev);   p_llr_dev = NULL; }
  if (p_out_dev)   { cudaFree(p_out_dev);   p_out_dev = NULL; }

  for (int i = 0; i < MAX_GRAPH_CACHE_SIZE; i++) {
    if (gpu_graph_cache[i].bridge_ptr) {
        cudaFreeHost(gpu_graph_cache[i].bridge_ptr);
        gpu_graph_cache[i].bridge_ptr = NULL;
    }
}
  for (int i = 0; i < 8; i++) {
    if (stream_bridges[i]) {
        cudaFreeHost(stream_bridges[i]);
        stream_bridges[i] = NULL;
    }
}

  for (int s = 0; s < 8; ++s) {
    if (decoder_streamsCreated) {
      cudaStreamDestroy(decoderStreams[s]);
    }
  }

  for (int s = 0; s < 4; s++) {
    if (encoder_streamsCreated) {
      cudaStreamDestroy(encoderStreams[s]);
    }
  }

  free_graphs();

  decoder_streamsCreated = false;
  encoder_streamsCreated = false;

  printf("[CUDA] Intermediate buffers and streams destroyed.\n");

  return 0;
}

int32_t LDPCdecoder_cuda(t_nrLDPC_dec_params* p_decParams,
                         int8_t* p_llr,
                         uint8_t* p_out,
                         t_nrLDPC_time_stats* p_profiler,
                         decode_abort_t* ab)
{
  if (!((p_decParams->R == 89 || p_decParams->R == 23 || p_decParams->R == 13) && p_decParams->BG == 1 && p_decParams->Z % 4 == 0
        && p_decParams->Z >= 128 && p_decParams->Z <= 384)) { // format check
    printf("Current format: BG = %d, R = %d, Zc = %d\n", p_decParams->BG, p_decParams->R, p_decParams->Z);
    AssertFatal(false, "Format cuda not support, only support BG = 1, Zc >= 128 and R = 13, 23, 89 right now\n");
    return 0;
  }
  // Launch LDPC decoder core for all segments
  int n_segments = p_decParams->n_segments;

  int numIter = nrLDPC_decoder_core_dynamic(p_llr, (int8_t*)p_out, n_segments, p_decParams, p_profiler, ab);

  set_abort(ab, false);

  return numIter;
}

/**
   \brief PerformsnrLDPC decoding of one code block
   \param p_llr Input LLRs
   \param p_out Output vector
   \param numLLR Number of LLRs
   \param p_decParamsnrLDPC decoder parameters
   \param p_profilernrLDPC profiler statistics
*/

static inline uint32_t nrLDPC_decoder_core_dynamic(int8_t* p_llr,
                                                   int8_t* p_out,
                                                   int n_segments,
                                                   t_nrLDPC_dec_params* p_decParams,
                                                   t_nrLDPC_time_stats* p_profiler,
                                                   decode_abort_t* ab)
{
  cudaError_t err_core = cudaSuccess;
  bool graph_executed = false;
  uint16_t Z = p_decParams->Z;
  uint8_t BG = p_decParams->BG;
  uint8_t R = p_decParams->R;
  uint8_t numMaxIter = p_decParams->numMaxIter;
  e_nrLDPC_outMode outMode = p_decParams->outMode;
  uint32_t K = Z * 22;
  // Calculate LLR size per segment based on Rate
  uint32_t numLLR = (R == 13) ? NR_LDPC_NCOL_BG1_R13 * Z : ((R == 89) ? NR_LDPC_NCOL_BG1_R89 * Z : NR_LDPC_NCOL_BG1_R23 * Z);
  if (p_llr != p_llr_dev)
    cudaMemcpyAsync(p_llr_dev, p_llr, n_segments * 68 * 384, cudaMemcpyHostToDevice, decoderStreams[0]);

  // Output size safety: assume worst-case unpacked bytes (K * n_segments)
  size_t total_output_size = n_segments * K * sizeof(int8_t);

/*
  // for debug, remember to remove it---------
  cuda_graph_breaker = 1; // skipping all the graph recording
  //-----------------------------------------
  */

  if (cuda_graph_breaker == 0) {
    int found_idx = -1;
    // Search in Graph Cache
    for (int i = 0; i < dynamic_cache_idx; i++) {
      if (gpu_graph_cache[i].occupied && gpu_graph_cache[i].Z == Z && gpu_graph_cache[i].R == R && gpu_graph_cache[i].BG == BG
          && gpu_graph_cache[i].K == K && gpu_graph_cache[i].numLLR == numLLR && gpu_graph_cache[i].numMaxIter == numMaxIter
          && gpu_graph_cache[i].n_segments == n_segments && gpu_graph_cache[i].outMode == outMode) {
        found_idx = i;
        break;
      }
    }
    if (found_idx >= 0) {
      // === Cache HIT: Execute Recorded Graph ===
      gpu_graph_cache[found_idx].bridge_ptr->p_llr_ptr = p_llr_dev;
      gpu_graph_cache[found_idx].bridge_ptr->p_out_ptr = (pageable || integrated) ? p_out : p_out_dev;

      err_core = nrLDPC_decoder_cuda_GraphExecute(gpu_graph_cache[found_idx].exec,
                                                  decoderStreams[0],
                                                  NULL, // doneEvent
                                                  0); // Stream Index
      if (err_core == cudaSuccess) {
        graph_executed = true;
      } else {
        cuda_graph_breaker = 1;
      }
    } else if (dynamic_cache_idx < MAX_GRAPH_CACHE_SIZE) {
      // === Cache MISS: Record New Graph and Execute ===
      int new_idx = dynamic_cache_idx;

      gpu_graph_cache[new_idx].occupied = true;
      gpu_graph_cache[new_idx].Z = Z;
      gpu_graph_cache[new_idx].R = R;
      gpu_graph_cache[new_idx].BG = BG;
      gpu_graph_cache[new_idx].K = K;
      gpu_graph_cache[new_idx].numLLR = numLLR;
      gpu_graph_cache[new_idx].numMaxIter = numMaxIter;
      gpu_graph_cache[new_idx].n_segments = n_segments;
      gpu_graph_cache[new_idx].outMode = outMode;
      // Use the determined pointers (Device ptrs for PCIe, Host ptrs for GH200)
      gpu_graph_cache[new_idx].bridge_ptr->p_llr_ptr = p_llr_dev;
      gpu_graph_cache[new_idx].bridge_ptr->p_out_ptr = pageable || integrated ? p_out : p_out_dev;

      err_core = nrLDPC_decoder_cuda_GraphRecord(gpu_graph_cache[new_idx].bridge_ptr,
                                                 numLLR,
                                                 cnProcBuf_dev,
                                                 bnProcBuf_dev,
                                                 llrRes_dev,
                                                 llrProcBuf_dev,
                                                 Z,
                                                 K,
                                                 BG,
                                                 R,
                                                 numMaxIter,
                                                 n_segments,
                                                 outMode,
                                                 decoderStreams,
                                                 0, // CudaStreamIdx
                                                 &gpu_graph_cache[new_idx].graph,
                                                 &gpu_graph_cache[new_idx].exec,
                                                 (uint8_t*)&gpu_graph_cache[new_idx].occupied);

      if (err_core == cudaSuccess) {
        err_core = nrLDPC_decoder_cuda_GraphExecute(gpu_graph_cache[new_idx].exec, decoderStreams[0], NULL, 0);

        if (err_core == cudaSuccess) {
          graph_executed = true;
          dynamic_cache_idx++;
        } else {
          cuda_graph_breaker = 1; // graph execution fail
        }
      } else {
        cuda_graph_breaker = 1; // graph record fail
      }
    }
  }

  if (!graph_executed) {
    // === Fallback to Normal Execution ===
    // If the cache is full, we cannot record new graphs.
    // Or graph operation is not safe in this device or environment.
    // Execute kernel directly using standard launch.
/*    if (cuda_graph_breaker == 1) {
      LOG_W(PHY, "Graph opereations failed, falling back to normal.\n");
    }*/
    ldpc_cuda_bridge_t* perpack_buffer = stream_bridges[0];
    perpack_buffer->p_llr_ptr = p_llr_dev;
    perpack_buffer->p_out_ptr = pageable || integrated ? p_out : p_out_dev;

    nrLDPC_decoder_cuda_NormalExecute(perpack_buffer,
                                      numLLR,
                                      cnProcBuf_dev,
                                      bnProcBuf_dev,
                                      llrRes_dev,
                                      llrProcBuf_dev,
                                      Z,
                                      K,
                                      BG,
                                      R,
                                      numMaxIter,
                                      n_segments,
                                      outMode,
                                      decoderStreams,
                                      0,
                                      NULL);
  }

  // Copy back and Cleanup for Discrete GPU
  if (!pageable && !integrated) {
    // Copy Output from Device to Host
    if (outMode == nrLDPC_outMode_BIT) {
      cudaMemcpyAsync(p_out, p_out_dev, total_output_size >> 3, cudaMemcpyDeviceToHost, decoderStreams[0]);
    }
    if (outMode == nrLDPC_outMode_BITINT8) {
      cudaMemcpyAsync(p_out, p_out_dev, total_output_size, cudaMemcpyDeviceToHost, decoderStreams[0]);
    }
  }

  cudaStreamSynchronize(decoderStreams[0]);
  if (p_decParams->check_crc) {
    for (int r = 0; r < n_segments; r++) {
      //      if (r<=1) for (int i=0;i<(K>>3);i++) printf("byte (%d,%d) %x\n",r,i,((uint8_t*)(p_out+r*(K>>3)))[i]);

      if (!p_decParams->check_crc((uint8_t*)(p_out + (r * (K >> 3))), p_decParams->Kprime, p_decParams->crc_type)) {
        LOG_D(PHY, "Segment %d/%d CRC NOK\n", r, n_segments);
        return (1 + numMaxIter);
      }
      /*
      uint8_t *b=(uint8_t*)(p_out + (r*(K>>3)));
      int i=0;
      if (b[K-2] == 0 && b[K - 1] == 0) {
            while (b[i] == 0 && i < K)
           i++;
            if (i == K) {
              LOG_E(PHY, "received all 0 pdu (K %d, r %d)\n",K,r);
            }
      }
      */
    }
  }
  return numMaxIter;
}

/* -------------------------------------------------------------------------------------------------------------------
 * Concurrent, early-terminating decoder used by the segment decoder (PDSCH/PUSCH).
 *
 * Contexts: the legacy path above has one set of device buffers and one graph cache, so every transport block (TB)
 * was decoded under a single global mutex. Here each context has its own processing buffers, input/output buffers,
 * deinterleaver/rate-recovery scratch, mapped bridge, CUDA stream (decoderStreams[ctx]), launch-dimension slot (the
 * Kdim_* arrays, indexed by stream) and graph cache. Up to nrLDPC_coding_cuda.num_contexts TBs are in flight.
 *
 * Early termination: every nrLDPC_coding_cuda.crc_check_interval iterations, a kernel checks the CRC of every code
 * block not yet decoded (the test of check_crc()); a code block that passes keeps that hard decision and is skipped by
 * every later kernel. The decode stops when all code blocks passed or the iteration budget (numMaxIter + 1
 * check/bit-node rounds, as the full-length path) is used up. See LDPCdecoder_cuda_ctx().
 * ------------------------------------------------------------------------------------------------------------------- */
#include <pthread.h>
#define LDPC_CUDA_MAXE (4 * 14 * 273 * 12 * 8)
_Static_assert(LDPC_ET_REC_SLOT >= LDPC_CUDA_MAX_CTX, "the recorder slot must not be a context slot");
_Static_assert(LDPC_CUDA_KDIM_SLOTS > LDPC_ET_REC_SLOT, "no launch-dimension slot for the recorder");
_Static_assert(LDPC_CUDA_MAX_CTX <= 8, "a context uses one of the 8 decoderStreams created by LDPCinit_cuda()");
// the CRC kernel's 8-byte loads of the posterior LLRs (code block r at r * NR_LDPC_MAX_NUM_LLR) must be aligned
_Static_assert(NR_LDPC_MAX_NUM_LLR % 8 == 0, "unaligned code blocks in llrRes");
// ldpc_cuda_et_state_t is shared with the .cu files, which use the fallback value of nrLDPC_CUDA_shared_param.h
_Static_assert(MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER == 36, "nrLDPC_CUDA_shared_param.h fallback out of date");

static ldpc_cuda_ctx_t ldpc_ctx[LDPC_CUDA_MAX_CTX];
static int ldpc_n_ctx = 0;
static int ldpc_chunk = 2;
static int ldpc_et_gpu = 1; // CRC checks and early termination on the GPU (nrLDPC_coding_cuda.crc_check)
static int ldpc_et_zero_copy = 0; // GPU coherent with the host (integrated, or ATS/NVLink-C2C): output written in place
// CRC combining factors per CRC type (CRC24_A, CRC24_B, CRC16) and bytes per thread L (see ldpc_et_crc_kernel())
static uint32_t* ldpc_et_xpow[3][LDPC_ET_MAX_L + 1];
static const uint32_t ldpc_et_crc_deg[3] = {24, 24, 16};
static const uint32_t ldpc_et_crc_low[3] = {0x864cfb, 0x800063, 0x1021};
// buckets of the number of code blocks a graph is recorded for: at most 1/3 padding above 4
static const uint8_t ldpc_et_buckets[] =
    {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4};

static void ldpc_et_recorder_start(void);
static void ldpc_et_recorder_stop(void);

// CUDA errors in the decoder are not recoverable: the output would be stale
#define LDPC_CUDA_CHECK(call)                                                             \
  do {                                                                                    \
    const cudaError_t e_ = (call);                                                        \
    AssertFatal(e_ == cudaSuccess, "CUDA LDPC: %s: %s\n", #call, cudaGetErrorString(e_)); \
  } while (0)

#define CTX_ALLOC(ptr, bytes)                                                            \
  do {                                                                                   \
    cudaError_t e_ = cudaMalloc((void**)&(ptr), (bytes));                                \
    AssertFatal(e_ == cudaSuccess, "cudaMalloc " #ptr ": %s\n", cudaGetErrorString(e_)); \
  } while (0)

void ldpc_cuda_ctx_init(void)
{
  if (ldpc_n_ctx)
    return;
  const ldpc_cuda_config_t* cfg = ldpc_cuda_get_config();
  const int n = cfg->num_contexts;
  AssertFatal(n >= 1 && n <= LDPC_CUDA_MAX_CTX, "%d decoder contexts, must be within [1, %d]\n", n, LDPC_CUDA_MAX_CTX);
  AssertFatal(cfg->crc_check_interval >= 1, "CRC check interval %d, must be at least 1\n", cfg->crc_check_interval);
  ldpc_chunk = cfg->crc_check_interval;
  ldpc_et_gpu = cfg->crc_check == LDPC_CUDA_CRC_CHECK_GPU;
  // pageable alone is not enough: x86 + HMM reports it for discrete GPUs, whose host memory is behind PCIe
  ldpc_et_zero_copy = integrated || pageable_uses_host;
  for (int type = 0; type < 3; type++) {
    const uint32_t deg = ldpc_et_crc_deg[type], low = ldpc_et_crc_low[type];
    for (int L = 1; L <= LDPC_ET_MAX_L; L++) {
      // thread t takes bytes [t L, (t+1) L) of the zero-padded message: factor x^(8 L (T-1-t)) mod g
      uint32_t xpow[LDPC_ET_THREADS];
      uint32_t v = 1;
      for (int t = LDPC_ET_THREADS - 1; t >= 0; t--) {
        xpow[t] = v;
        for (int k = 0; k < 8 * L; k++) {
          v <<= 1;
          if (v & (1u << deg))
            v ^= (1u << deg) | low;
        }
      }
      CTX_ALLOC(ldpc_et_xpow[type][L], sizeof(xpow));
      LDPC_CUDA_CHECK(cudaMemcpy(ldpc_et_xpow[type][L], xpow, sizeof(xpow), cudaMemcpyHostToDevice));
    }
  }
  const size_t nseg = MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4;
  for (int i = 0; i < n; i++) {
    ldpc_cuda_ctx_t* c = &ldpc_ctx[i];
    pthread_mutex_init(&c->mutex, NULL);
    CTX_ALLOC(c->cnProcBuf, nseg * NR_LDPC_SIZE_CN_PROC_BUF);
    CTX_ALLOC(c->bnProcBuf, nseg * NR_LDPC_SIZE_BN_PROC_BUF);
    CTX_ALLOC(c->llrRes, nseg * NR_LDPC_MAX_NUM_LLR);
    CTX_ALLOC(c->llrProcBuf, nseg * NR_LDPC_MAX_NUM_LLR);
    CTX_ALLOC(c->llr, nseg * 68 * 384);
    LDPC_CUDA_CHECK(cudaMemset(c->llr, 0, nseg * 68 * 384));
    CTX_ALLOC(c->out, nseg * NR_LDPC_MAX_NUM_LLR);
    CTX_ALLOC(c->harq_e, LDPC_CUDA_MAXE * sizeof(int16_t));
    c->harq_f = NULL;
    if (!pageable && !integrated)
      CTX_ALLOC(c->harq_f, LDPC_CUDA_MAXE * sizeof(int16_t));
    cudaError_t e = cudaHostAlloc((void**)&c->out_host, nseg * NR_LDPC_MAX_NUM_LLR, cudaHostAllocDefault);
    AssertFatal(e == cudaSuccess, "cudaHostAlloc out_host: %s\n", cudaGetErrorString(e));
    e = cudaHostAlloc((void**)&c->bridge, sizeof(ldpc_cuda_bridge_t), cudaHostAllocMapped);
    AssertFatal(e == cudaSuccess, "cudaHostAlloc bridge: %s\n", cudaGetErrorString(e));
    c->bridge->p_llr_ptr = c->llr;
    c->bridge->p_out_ptr = c->out;
    c->n_graphs = 0;
    CTX_ALLOC(c->st, sizeof(ldpc_cuda_et_state_t));
    const size_t et_bytes = LDPC_ET_PASS_IT_BYTES + nseg * NR_LDPC_MAX_NUM_LLR;
    e = cudaHostAlloc((void**)&c->et_host, et_bytes, ldpc_et_zero_copy ? cudaHostAllocMapped : cudaHostAllocDefault);
    AssertFatal(e == cudaSuccess, "cudaHostAlloc et_host: %s\n", cudaGetErrorString(e));
    if (ldpc_et_zero_copy) {
      e = cudaHostGetDevicePointer((void**)&c->et_dev, c->et_host, 0);
      AssertFatal(e == cudaSuccess, "cudaHostGetDevicePointer et_host: %s\n", cudaGetErrorString(e));
    } else {
      CTX_ALLOC(c->et_dev, et_bytes);
    }
    c->et_copy = !ldpc_et_zero_copy;
    c->lane.streams = decoderStreams;
    c->lane.slot = i;
    e = cudaStreamCreateWithFlags(&c->lane.side_stream, cudaStreamNonBlocking);
    AssertFatal(e == cudaSuccess, "cudaStreamCreate side_stream: %s\n", cudaGetErrorString(e));
    e = cudaEventCreateWithFlags(&c->lane.ev_fork, cudaEventDisableTiming);
    AssertFatal(e == cudaSuccess, "cudaEventCreate: %s\n", cudaGetErrorString(e));
    e = cudaEventCreateWithFlags(&c->lane.ev_join, cudaEventDisableTiming);
    AssertFatal(e == cudaSuccess, "cudaEventCreate: %s\n", cudaGetErrorString(e));
    e = cudaEventCreateWithFlags(&c->ev_first, cudaEventDisableTiming);
    AssertFatal(e == cudaSuccess, "cudaEventCreate: %s\n", cudaGetErrorString(e));
    c->n_et_graphs = 0;
    pthread_mutex_init(&c->et_graphs_mutex, NULL);
    // warm-up: one short chunk on this context's stream (driver/JIT initialisation)
    nrLDPC_decoder_cuda_NormalExecuteChunk(c, 384, 23, NR_LDPC_NCOL_BG1_R23 * 384, 1, 1, 1);
    LDPC_CUDA_CHECK(cudaStreamSynchronize(decoderStreams[i]));
  }
  ldpc_n_ctx = n;
  ldpc_et_recorder_start();
  LOG_I(NR_PHY,
        "CUDA LDPC: %d concurrent decoder contexts, early termination every %d iteration(s), CRC on the %s%s\n",
        n,
        ldpc_chunk,
        ldpc_et_gpu ? "GPU" : "host",
        ldpc_et_gpu ? (ldpc_et_zero_copy ? ", output zero-copy" : ", output copied") : "");
}

// Stop the background recorder and free the decoder contexts; ldpc_cuda_ctx_init() may be called again afterwards. The
// caller must have stopped decoding: this waits for the decodes in progress, but a later ldpc_cuda_ctx_acquire() fails.
void ldpc_cuda_ctx_shutdown(void)
{
  if (!ldpc_n_ctx)
    return;
  for (int i = 0; i < ldpc_n_ctx; i++) // wait for the decodes in progress, which may also queue recorder requests
    pthread_mutex_lock(&ldpc_ctx[i].mutex);
  ldpc_et_recorder_stop();
  for (int i = 0; i < ldpc_n_ctx; i++) {
    ldpc_cuda_ctx_t* c = &ldpc_ctx[i];
    cudaStreamSynchronize(decoderStreams[i]);
    for (int g = 0; g < c->n_graphs; g++) {
      cudaGraphExecDestroy(c->graphs[g].exec);
      cudaGraphDestroy(c->graphs[g].graph);
    }
    for (int g = 0; g < c->n_et_graphs; g++) {
      for (int k = 0; k < 2; k++) {
        if (c->et_graphs[g].exec[k])
          cudaGraphExecDestroy(c->et_graphs[g].exec[k]);
        if (c->et_graphs[g].graph[k])
          cudaGraphDestroy(c->et_graphs[g].graph[k]);
      }
    }
    cudaFree(c->cnProcBuf);
    cudaFree(c->bnProcBuf);
    cudaFree(c->llrRes);
    cudaFree(c->llrProcBuf);
    cudaFree(c->llr);
    cudaFree(c->out);
    cudaFree(c->harq_e);
    cudaFree(c->harq_f);
    cudaFree(c->st);
    if (!ldpc_et_zero_copy)
      cudaFree(c->et_dev);
    cudaFreeHost(c->et_host);
    cudaFreeHost(c->out_host);
    cudaFreeHost(c->bridge);
    cudaStreamDestroy(c->lane.side_stream);
    cudaEventDestroy(c->lane.ev_fork);
    cudaEventDestroy(c->lane.ev_join);
    cudaEventDestroy(c->ev_first);
    pthread_mutex_destroy(&c->et_graphs_mutex);
    pthread_mutex_unlock(&c->mutex);
    pthread_mutex_destroy(&c->mutex);
    memset(c, 0, sizeof(*c));
  }
  for (int type = 0; type < 3; type++) {
    for (int L = 1; L <= LDPC_ET_MAX_L; L++) {
      cudaFree(ldpc_et_xpow[type][L]);
      ldpc_et_xpow[type][L] = NULL;
    }
  }
  ldpc_n_ctx = 0;
}

int ldpc_cuda_ctx_acquire(void)
{
  AssertFatal(ldpc_n_ctx > 0, "CUDA LDPC decoder contexts not initialized\n");
  static unsigned int rr = 0; // unsigned: wraps around without becoming negative
  const unsigned int start = __sync_fetch_and_add(&rr, 1);
  for (int k = 0; k < ldpc_n_ctx; k++) {
    const int i = (start + k) % ldpc_n_ctx;
    if (pthread_mutex_trylock(&ldpc_ctx[i].mutex) == 0)
      return i;
  }
  const int i = start % ldpc_n_ctx;
  pthread_mutex_lock(&ldpc_ctx[i].mutex);
  return i;
}

void ldpc_cuda_ctx_release(int ci)
{
  pthread_mutex_unlock(&ldpc_ctx[ci].mutex);
}

int8_t* ldpc_cuda_ctx_llr(int ci)
{
  return ldpc_ctx[ci].llr;
}
int16_t* ldpc_cuda_ctx_harq_e(int ci)
{
  return ldpc_ctx[ci].harq_e;
}
int16_t* ldpc_cuda_ctx_harq_f(int ci)
{
  return ldpc_ctx[ci].harq_f;
}

static void ldpc_ctx_run_chunk(ldpc_cuda_ctx_t* c, uint32_t Z, uint8_t R, uint32_t numLLR, uint8_t C, int prologue, int iters)
{
  cudaStream_t stream = c->lane.streams[c->lane.slot];
  if (cuda_graph_breaker == 0) {
    for (int g = 0; g < c->n_graphs; g++) {
      ldpc_chunk_graph_t* x = &c->graphs[g];
      if (x->Z == Z && x->R == R && x->numLLR == numLLR && x->n_segments == C && x->prologue == prologue && x->iters == iters) {
        if (cudaGraphLaunch(x->exec, stream) == cudaSuccess)
          return;
        break;
      }
    }
    if (c->n_graphs < LDPC_CUDA_CTX_GRAPHS) {
      ldpc_chunk_graph_t* x = &c->graphs[c->n_graphs];
      if (nrLDPC_decoder_cuda_GraphRecordChunk(c, Z, R, numLLR, C, prologue, iters, &x->graph, &x->exec) == cudaSuccess) {
        x->Z = Z;
        x->R = R;
        x->numLLR = numLLR;
        x->n_segments = C;
        x->prologue = prologue;
        x->iters = iters;
        c->n_graphs++;
        if (cudaGraphLaunch(x->exec, stream) == cudaSuccess)
          return;
      }
    }
  }
  cudaGetLastError(); // a failed graph recording or launch, if any: launched directly instead
  nrLDPC_decoder_cuda_NormalExecuteChunk(c, Z, R, numLLR, C, prologue, iters);
}

static int ldpc_et_bucket(int C)
{
  AssertFatal(C >= 1 && C <= MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER * 4, "%d code blocks\n", C);
  int i = 0;
  while (ldpc_et_buckets[i] < C)
    i++;
  return ldpc_et_buckets[i];
}

/* Background graph recorder: records the graphs of the shapes queued by ldpc_ctx_et_graph(), off the decoding threads.
   It captures on its own stream, side stream, events and launch-dimension slot (LDPC_ET_REC_SLOT), so it never interferes
   with a decode in progress on the context; capture launches nothing. Started by ldpc_cuda_ctx_init(), stopped by
   ldpc_cuda_ctx_shutdown(). */
#define LDPC_ET_REC_QUEUE 64
static struct {
  pthread_t thread;
  bool running; // cleared to stop the recorder, protected by mutex
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  struct {
    ldpc_cuda_ctx_t* c;
    ldpc_et_graph_t* x;
  } q[LDPC_ET_REC_QUEUE];
  int head, n;
  cudaStream_t streams[LDPC_ET_REC_SLOT + 1]; // streams[LDPC_ET_REC_SLOT]: its stream
  ldpc_cuda_lane_t lane; // its stream, launch-dimension slot (LDPC_ET_REC_SLOT), side stream and events
} ldpc_et_rec;

// Record the graphs of shape x of context c (see ldpc_ctx_et_graph()) and publish them in the context's cache
static void ldpc_et_record(ldpc_cuda_ctx_t* c, ldpc_et_graph_t* x)
{
  ldpc_et_graph_t rec = *x; // the shape, constant while pending; recorded here, published under the lock
  cudaError_t err = nrLDPC_decoder_cuda_EnqueueET(c, &rec, &ldpc_et_rec.lane, true);
  cudaGraphExec_t* exec = rec.exec;
  for (int g = 0; g < 2 && err == cudaSuccess; g++) // upload now rather than at the first launch
    if (exec[g])
      err = cudaGraphUpload(exec[g], ldpc_et_rec.streams[LDPC_ET_REC_SLOT]);
  if (err == cudaSuccess)
    err = cudaStreamSynchronize(ldpc_et_rec.streams[LDPC_ET_REC_SLOT]);
  if (err != cudaSuccess) {
    LOG_W(NR_PHY, "CUDA LDPC: graph recording failed (Z %u R %d C %d): %s\n", x->Z, x->R, x->n_segments, cudaGetErrorString(err));
    cudaGetLastError();
  }
  pthread_mutex_lock(&c->et_graphs_mutex);
  memcpy(x->graph, rec.graph, sizeof(rec.graph));
  memcpy(x->exec, rec.exec, sizeof(rec.exec));
  x->state = err == cudaSuccess ? ET_GRAPH_READY : ET_GRAPH_FAILED;
  pthread_mutex_unlock(&c->et_graphs_mutex);
}

static void* ldpc_et_recorder(void* arg)
{
  (void)arg;
  pthread_mutex_lock(&ldpc_et_rec.mutex);
  while (ldpc_et_rec.running) {
    if (ldpc_et_rec.n == 0) {
      pthread_cond_wait(&ldpc_et_rec.cond, &ldpc_et_rec.mutex);
      continue;
    }
    ldpc_cuda_ctx_t* c = ldpc_et_rec.q[ldpc_et_rec.head].c;
    ldpc_et_graph_t* x = ldpc_et_rec.q[ldpc_et_rec.head].x;
    ldpc_et_rec.head = (ldpc_et_rec.head + 1) % LDPC_ET_REC_QUEUE;
    ldpc_et_rec.n--;
    pthread_mutex_unlock(&ldpc_et_rec.mutex);
    ldpc_et_record(c, x);
    pthread_mutex_lock(&ldpc_et_rec.mutex);
  }
  // stopped: requests still queued are dropped, their contexts are being freed
  pthread_mutex_unlock(&ldpc_et_rec.mutex);
  return NULL;
}

static void ldpc_et_recorder_start(void)
{
  pthread_mutex_init(&ldpc_et_rec.mutex, NULL);
  pthread_cond_init(&ldpc_et_rec.cond, NULL);
  cudaError_t e = cudaStreamCreateWithFlags(&ldpc_et_rec.streams[LDPC_ET_REC_SLOT], cudaStreamNonBlocking);
  AssertFatal(e == cudaSuccess, "cudaStreamCreate: %s\n", cudaGetErrorString(e));
  ldpc_et_rec.lane.streams = ldpc_et_rec.streams;
  ldpc_et_rec.lane.slot = LDPC_ET_REC_SLOT;
  e = cudaStreamCreateWithFlags(&ldpc_et_rec.lane.side_stream, cudaStreamNonBlocking);
  AssertFatal(e == cudaSuccess, "cudaStreamCreate: %s\n", cudaGetErrorString(e));
  e = cudaEventCreateWithFlags(&ldpc_et_rec.lane.ev_fork, cudaEventDisableTiming);
  AssertFatal(e == cudaSuccess, "cudaEventCreate: %s\n", cudaGetErrorString(e));
  e = cudaEventCreateWithFlags(&ldpc_et_rec.lane.ev_join, cudaEventDisableTiming);
  AssertFatal(e == cudaSuccess, "cudaEventCreate: %s\n", cudaGetErrorString(e));
  ldpc_et_rec.head = 0;
  ldpc_et_rec.n = 0;
  ldpc_et_rec.running = true;
  // not threadCreate(): not available to every program loading the library (e.g. ldpctest)
  AssertFatal(pthread_create(&ldpc_et_rec.thread, NULL, ldpc_et_recorder, NULL) == 0, "pthread_create failed\n");
  pthread_setname_np(ldpc_et_rec.thread, "ldpc_cuda_rec");
}

static void ldpc_et_recorder_stop(void)
{
  pthread_mutex_lock(&ldpc_et_rec.mutex);
  ldpc_et_rec.running = false;
  pthread_cond_signal(&ldpc_et_rec.cond);
  pthread_mutex_unlock(&ldpc_et_rec.mutex);
  pthread_join(ldpc_et_rec.thread, NULL);
  cudaStreamDestroy(ldpc_et_rec.streams[LDPC_ET_REC_SLOT]);
  cudaStreamDestroy(ldpc_et_rec.lane.side_stream);
  cudaEventDestroy(ldpc_et_rec.lane.ev_fork);
  cudaEventDestroy(ldpc_et_rec.lane.ev_join);
  pthread_cond_destroy(&ldpc_et_rec.cond);
  pthread_mutex_destroy(&ldpc_et_rec.mutex);
}

/* Graphs of the early-terminating decode for Cb code blocks if recorded, NULL otherwise: decode with direct launches.
   A shape seen for the first time is queued for the background recorder. The cache keeps the most recently used ones. */
static const ldpc_et_graph_t*
ldpc_ctx_et_graph(ldpc_cuda_ctx_t* c, uint32_t Z, uint8_t R, uint32_t numLLR, int Cb, int budget, int chunk)
{
  pthread_mutex_lock(&c->et_graphs_mutex);
  c->et_uses++;
  ldpc_et_graph_t* lru = NULL;
  for (int g = 0; g < c->n_et_graphs; g++) {
    ldpc_et_graph_t* x = &c->et_graphs[g];
    if (x->Z == Z && x->R == R && x->n_segments == Cb && x->budget == budget && x->chunk == chunk) {
      x->last_use = c->et_uses;
      pthread_mutex_unlock(&c->et_graphs_mutex);
      return x->state == ET_GRAPH_READY ? x : NULL;
    }
    if (x->state != ET_GRAPH_PENDING && (!lru || x->last_use < lru->last_use))
      lru = x;
  }
  // queue a request for the recorder, unless it is stopped or its queue is full: the capacity check and the request are
  // made under the same lock, several decoding threads may queue at the same time (lock order: et_graphs_mutex, then
  // ldpc_et_rec.mutex; the recorder never holds both)
  pthread_mutex_lock(&ldpc_et_rec.mutex);
  ldpc_et_graph_t* x = NULL;
  if (ldpc_et_rec.running && ldpc_et_rec.n < LDPC_ET_REC_QUEUE) {
    if (c->n_et_graphs < LDPC_ET_CTX_GRAPHS) {
      x = &c->et_graphs[c->n_et_graphs++];
    } else if (lru) { // evict: the previous decode on this context may still run its graphs
      x = lru;
      LDPC_CUDA_CHECK(cudaStreamSynchronize(c->lane.streams[c->lane.slot]));
      for (int g = 0; g < 2; g++) {
        if (x->exec[g])
          cudaGraphExecDestroy(x->exec[g]);
        if (x->graph[g])
          cudaGraphDestroy(x->graph[g]);
      }
    }
  }
  if (x) {
    *x = (ldpc_et_graph_t){.Z = Z,
                           .R = R,
                           .n_segments = Cb,
                           .budget = budget,
                           .chunk = chunk,
                           .numLLR = numLLR,
                           .state = ET_GRAPH_PENDING,
                           .last_use = c->et_uses};
    const int tail = (ldpc_et_rec.head + ldpc_et_rec.n) % LDPC_ET_REC_QUEUE;
    ldpc_et_rec.q[tail].c = c;
    ldpc_et_rec.q[tail].x = x;
    ldpc_et_rec.n++;
    pthread_cond_signal(&ldpc_et_rec.cond);
  }
  pthread_mutex_unlock(&ldpc_et_rec.mutex);
  pthread_mutex_unlock(&c->et_graphs_mutex);
  return NULL;
}

/* Decode the C code blocks in context ci (input already in ldpc_cuda_ctx_llr(ci)). p_out receives C * K/8 bytes: the
   hard decision of each code block from the CRC check where it first passed, else the last one. passed[r] tells whether
   code block r passed its CRC. Returns the number of code blocks that passed. */
int LDPCdecoder_cuda_ctx(int ci, t_nrLDPC_dec_params* p_decParams, uint8_t* p_out, bool* passed)
{
  ldpc_cuda_ctx_t* c = &ldpc_ctx[ci];
  const uint32_t Z = p_decParams->Z;
  const uint8_t R = p_decParams->R;
  const uint8_t C = p_decParams->n_segments;
  const uint32_t K = 22 * Z;
  const uint32_t Kb = K >> 3;
  const uint32_t numLLR = (R == 13) ? NR_LDPC_NCOL_BG1_R13 * Z : ((R == 89) ? NR_LDPC_NCOL_BG1_R89 * Z : NR_LDPC_NCOL_BG1_R23 * Z);
  const int budget = p_decParams->numMaxIter + 1;
  const int chunk = ldpc_chunk < budget ? ldpc_chunk : budget;
  const int crc =
      p_decParams->crc_type == CRC24_A ? 0 : (p_decParams->crc_type == CRC24_B ? 1 : (p_decParams->crc_type == CRC16 ? 2 : -1));
  cudaStream_t stream = c->lane.streams[c->lane.slot];
  int n_passed = 0;
  if (ldpc_et_gpu && crc >= 0 && !(p_decParams->Kprime & 7)) {
    const int Cb = ldpc_et_bucket(C);
    const ldpc_et_graph_t* et = cuda_graph_breaker == 0 ? ldpc_ctx_et_graph(c, Z, R, numLLR, Cb, budget, chunk) : NULL;
    const uint32_t nbytes = p_decParams->Kprime >> 3;
    nrLDPC_decoder_cuda_ETSetup(c,
                                C,
                                Cb,
                                nbytes,
                                ldpc_et_crc_deg[crc],
                                ldpc_et_crc_low[crc],
                                ldpc_et_xpow[crc][(nbytes + LDPC_ET_THREADS - 1) / LDPC_ET_THREADS]);
    LDPC_CUDA_CHECK(cudaGetLastError());
    // enqueue the whole decode, first chunk then the remaining ones, but first wait for the first one only: done if
    // every code block passed
    if (et) {
      LDPC_CUDA_CHECK(cudaGraphLaunch(et->exec[0], stream));
      if (et->exec[1]) {
        LDPC_CUDA_CHECK(cudaEventRecord(c->ev_first, stream));
        LDPC_CUDA_CHECK(cudaGraphLaunch(et->exec[1], stream));
      }
    } else {
      ldpc_et_graph_t shape = {.Z = Z, .R = R, .n_segments = Cb, .budget = budget, .chunk = chunk, .numLLR = numLLR};
      LDPC_CUDA_CHECK(nrLDPC_decoder_cuda_EnqueueET(c, &shape, &c->lane, false));
    }
    // the second part may still run after an early return: it leaves the output of passed code blocks unchanged
    const int32_t* pass_it = (const int32_t*)c->et_host;
    bool all_passed = false;
    if (budget > chunk) {
      LDPC_CUDA_CHECK(cudaEventSynchronize(c->ev_first));
      int r = 0;
      while (r < C && pass_it[r] > 0)
        r++;
      all_passed = r == C;
    }
    if (!all_passed)
      LDPC_CUDA_CHECK(cudaStreamSynchronize(stream));
    for (int r = 0; r < C; r++) {
      passed[r] = pass_it[r] > 0;
      n_passed += passed[r];
    }
    memcpy(p_out, c->et_host + LDPC_ET_PASS_IT_BYTES, (size_t)C * Kb);
    return n_passed;
  }
  // CRC on the host (nrLDPC_coding_cuda.crc_check host, or a CRC the GPU check does not handle)
  memset(passed, 0, C * sizeof(*passed));
  for (int it = 0, k; it < budget && n_passed < C; it += k) {
    k = (budget - it) < ldpc_chunk ? (budget - it) : ldpc_chunk;
    ldpc_ctx_run_chunk(c, Z, R, numLLR, C, it == 0, k);
    LDPC_CUDA_CHECK(cudaGetLastError());
    LDPC_CUDA_CHECK(cudaMemcpyAsync(c->out_host, c->out, (size_t)C * Kb, cudaMemcpyDeviceToHost, stream));
    LDPC_CUDA_CHECK(cudaStreamSynchronize(stream));
    for (int r = 0; r < C; r++) {
      if (passed[r])
        continue;
      if (p_decParams->check_crc(c->out_host + (size_t)r * Kb, p_decParams->Kprime, p_decParams->crc_type)) {
        memcpy(p_out + (size_t)r * Kb, c->out_host + (size_t)r * Kb, Kb);
        passed[r] = true;
        n_passed++;
      }
    }
  }
  for (int r = 0; r < C; r++) // code blocks that never passed: last hard decision
    if (!passed[r])
      memcpy(p_out + (size_t)r * Kb, c->out_host + (size_t)r * Kb, Kb);
  return n_passed;
}
