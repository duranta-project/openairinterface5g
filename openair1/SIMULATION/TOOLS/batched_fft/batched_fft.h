/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// batched_fft: a deliberately limited float32 complex FFT that computes B independent transforms
// of the same power-of-two length N per call, one per SIMD lane.
//
// ---------------------------------------------------------------------------------------------
// What one call computes
// ---------------------------------------------------------------------------------------------
// B = batched_fft_batch_size(isa): 1 (scalar), 8 (AVX2), 16 (AVX512), 4 (NEON), vector length
// in floats for SVE2 (4 at 128-bit VL). A call always transforms all B sequences.
//
// Memory: two float arrays, re and im, of B * N floats each ("lane-major"). Complex element n of
// sequence b (0 <= n < N, 0 <= b < B) is
//
//     re[n * B + b] + i * im[n * B + b]
//
// i.e. the B sequences are interleaved element by element. For AVX2 (B = 8), N = 4:
//
//     re: [x0[0] x1[0] ... x7[0] | x0[1] x1[1] ... x7[1] | x0[2] ... | x0[3] ... x7[3]]
//
// Every sequence is independent: no value ever crosses from one b to another, so unused sequences
// may hold anything (including NaN) without affecting the others.
//
// Both transforms work in place on re/im. re and im must not overlap. tw must be
// batched_fft_twiddles(N) for the same N. N must be a power of two, 2 <= N <= 2^BATCHED_FFT_MAX_LOG2.
// Nothing is checked at runtime.
//
// ---------------------------------------------------------------------------------------------
// Forward: batched_fft_forward*()
// ---------------------------------------------------------------------------------------------
//   input  (natural order):      position n holds x_b[n]
//   output (bit-reversed order): position p holds X_b[bitrev(p)]
//
//   X_b[k] = sum_{n=0}^{N-1} x_b[n] * e^{-2 pi i n k / N}          (unscaled)
//
// where bitrev(p) reverses the log2(N) bits of p. For N = 8 the output positions 0..7 hold
// X[0] X[4] X[2] X[6] X[1] X[5] X[3] X[7]. bitrev is its own inverse, so the natural-order
// spectrum is X_b[k] = value at position bitrev(k).
//
// ---------------------------------------------------------------------------------------------
// Inverse: batched_fft_inverse*()
// ---------------------------------------------------------------------------------------------
//   input  (bit-reversed order, i.e. the forward output format): position p holds Y_b[bitrev(p)]
//   output (natural order):                                       position n holds y_b[n]
//
//   y_b[n] = sum_{k=0}^{N-1} Y_b[k] * e^{+2 pi i n k / N}          (unscaled)
//
// So inverse(forward(x)) = N * x. Neither direction applies 1/N or 1/sqrt(N).
//
// Pointwise operations do not care about the order, so a circular convolution needs no bit
// reversal anywhere: forward(x), forward(h) (e.g. cached), multiply position by position,
// inverse, divide by N (fold 1/N into h).
//
// ---------------------------------------------------------------------------------------------
// Compared with OAI's dft()/idft() (openair1/PHY/TOOLS)
// ---------------------------------------------------------------------------------------------
// Same sign convention (forward e^-, inverse e^+). Different in everything else: OAI is one
// transform per call, int16 Q15 interleaved c16_t, out of place, natural order in and out, many
// sizes including non-powers of two, and always scaled by 1/sqrt(N). batched_fft is B transforms
// per call, float planar, in place, bit-reversed spectrum, powers of two only, unscaled.
//
// ---------------------------------------------------------------------------------------------
// Accuracy, performance, threading
// ---------------------------------------------------------------------------------------------
// float32 throughout; twiddles are computed in double. Error relative to the RMS of the result is
// ~1e-7 (about 140 dB SNR) up to N = 4096; tests bound it at 1e-6 * (log2(N) + 2).
//
// Radix-2^2 (two radix-2 stages per memory pass), plus one twiddle-free radix-2 pass for odd
// log2(N). Transforms whose re+im exceed ~32 KB recurse depth-first so each quarter finishes in
// L1. Loads and stores tolerate any alignment, but keep re/im 64-byte aligned: with AVX512 an
// unaligned array splits a cache line on every access and runs ~1.8x slower.
//
// The transforms have no state and are reentrant. batched_fft_twiddles() builds all tables on
// its first call (thread-safe) and returns shared read-only memory valid for the whole process.
//
// ISA: each ISA has its own source file (batched_fft_<isa>.c) and direct entry points
// (batched_fft_forward_avx512(), ...) for callers that already dispatched; batched_fft_forward()
// switches on the ISA. Only call an ISA for which batched_fft_isa_supported() is true.
// x86: AVX2 and AVX512F kernels are always built (per-file compiler flags) and picked at runtime.
// aarch64: NEON is baseline; SVE2 is built when the compiler targets it, or with GCC 13+ from a
// non-SVE baseline (BATCHED_FFT_HAVE_SVE2).

#ifndef BATCHED_FFT_H
#define BATCHED_FFT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATCHED_FFT_MAX_LOG2 12
// Transforms whose re + im exceed this many floats (~32 KB) recurse depth-first so each quarter
// finishes while it is still in L1.
#define BATCHED_FFT_LEAF_FLOATS 4096

#if defined(__aarch64__) && (defined(__ARM_FEATURE_SVE2) || (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13))
#define BATCHED_FFT_HAVE_SVE2 1
#endif

typedef enum {
  BATCHED_FFT_ISA_SCALAR = 1,
  BATCHED_FFT_ISA_AVX2,
  BATCHED_FFT_ISA_AVX512,
  BATCHED_FFT_ISA_NEON,
  BATCHED_FFT_ISA_SVE2,
} batched_fft_isa_t;

/// Kernels for the ISA are compiled in and this CPU supports them.
bool batched_fft_isa_supported(batched_fft_isa_t isa);
/// B, the number of transforms per call (the vector width in floats); 1 for scalar, runtime for SVE.
int batched_fft_batch_size(batched_fft_isa_t isa);
/// Twiddle table for size N: N/2 values e^{-2 pi i k / N}, real parts first, then imaginary parts.
/// NULL if N is not a power of two in [2, 2^BATCHED_FFT_MAX_LOG2].
const float *batched_fft_twiddles(int N);

/// Forward transform of B sequences in place: natural order in, bit-reversed order out, unscaled.
void batched_fft_forward(batched_fft_isa_t isa, float *re, float *im, int N, const float *tw);
/// Inverse transform of B sequences in place: bit-reversed order in, natural order out, unscaled.
void batched_fft_inverse(batched_fft_isa_t isa, float *re, float *im, int N, const float *tw);

// Direct per-ISA entry points, same contract as above.

void batched_fft_forward_scalar(float *re, float *im, int N, const float *tw);
void batched_fft_inverse_scalar(float *re, float *im, int N, const float *tw);
#if defined(__x86_64__)
void batched_fft_forward_avx2(float *re, float *im, int N, const float *tw);
void batched_fft_inverse_avx2(float *re, float *im, int N, const float *tw);
void batched_fft_forward_avx512(float *re, float *im, int N, const float *tw);
void batched_fft_inverse_avx512(float *re, float *im, int N, const float *tw);
#endif
#if defined(__aarch64__)
void batched_fft_forward_neon(float *re, float *im, int N, const float *tw);
void batched_fft_inverse_neon(float *re, float *im, int N, const float *tw);
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
void batched_fft_forward_sve2(float *re, float *im, int N, const float *tw);
void batched_fft_inverse_sve2(float *re, float *im, int N, const float *tw);
int batched_fft_batch_size_sve2(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
