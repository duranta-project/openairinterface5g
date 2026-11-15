/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <stdatomic.h>
#include <string.h>
#include <simde/x86/avx2.h>
#include <simde/x86/fma.h>

#include  "sim.h"

static inline void tableNor(unsigned long seed);
static unsigned int urseed, iy, ir[98]; /// uniformrandom
static bool tableNordDone = false; /// gaussZiggurat
static uint64_t gauss_noise_seed = 0x9E3779B97F4A7C15ull; /// gauss_noise_int16

/*!\brief Generate a random number form `/dev/urandom`. */
void fill_random(void *buf, size_t sz)
{
  const char* fn = "/dev/urandom";
  FILE* f = fopen(fn, "rb");
  if (f == NULL) {
    fprintf(stderr, "could not open %s for seed generation: %d %s\n", fn, errno, strerror(errno));
    abort();
  }
  int rc = fread(buf, sz, 1, f);
  if (rc < 0) {
    fprintf(stderr, "could not read %s for seed generation: %d %s\n", fn, errno, strerror(errno));
    abort();
  }
  fclose(f);
}


static const unsigned int a = 1664525lu;

/*!\brief Initialization routine for Uniform/Gaussian random number generators. */
// fix seed number by env variable OAI_RNGSEED
void randominit()
{
  unsigned long seed;
  const char* str_oai_rngseed = getenv("OAI_RNGSEED");
  if (str_oai_rngseed != NULL) {
    char *end;
    seed = strtoul(str_oai_rngseed, &end, 0); /* 0: any base in hex, dec, oct */
    AssertFatal(*end == '\0' || *end == '\n',
                "could not read seed %s at char '%c' (%d), pos %ld\n",
                str_oai_rngseed,
                *end,
                *end,
                end - str_oai_rngseed);
  } else {
    fill_random(&seed, sizeof(seed));
  }
  printf("Initializing random number generator, seed %lu\n", seed);

  // initialize uniformrandom RNG
  urseed = (unsigned int) seed;
  if (urseed % 2 == 0)
    urseed += 1; /* urseed and mod are relative prime */
  for (int i = 1; i <= 97; i++) {
    urseed = a * urseed; /* mod 2**32  */
    ir[i] = urseed; /* initialize the shuffle table    */
  }
  iy = 1;

  // initialize gaussZiggurat RNG
  tableNor(seed);

  // gauss_noise_int16: the threads that already drew noise keep their sequence
  gauss_noise_seed = seed;
}

/*
@defgroup _uniformdouble
@ingroup numerical Uniform linear congruential random number generator.
*/

/*!\brief Uniform linear congruential random number generator on \f$[0,1)\f$.  Returns a double-precision floating-point number.*/

double uniformrandom(void)
{
  const double mod = 4294967296.0; /* is 2**32 */
  int j = 1 + 97.0 * iy / mod;
  iy = ir[j];
  urseed = a * urseed; /* mod 2**32 */
  ir[j] = urseed;
  return (double)iy / mod;
}

/*
@defgroup _gaussdouble Gaussian random number generator based on modified Box-Muller transformation.
@ingroup numerical
*/

/*!\brief Gaussian random number generator based on modified Box-Muller transformation.Returns a double-precision floating-point number. */

double __attribute__ ((no_sanitize_address)) gaussdouble(double mean, double variance)
{
  static int iset=0;
  static double gset;
  double fac,r,v1,v2;

  if (iset == 0) {
    do {
      v1 = 2.0*uniformrandom()-1.0;
      v2 = 2.0*uniformrandom()-1.0;
      r = v1*v1+v2*v2;
    }  while (r >= 1.0);

    fac = sqrt(-2.0*log(r)/r);
    gset= v1*fac;
    iset=1;
    return(sqrt(variance)*v2*fac + mean);
  } else {
    iset=0;
    return(sqrt(variance)*gset + mean);
  }
}


/*
@defgroup _gaussZiggurat
@ingroup numerical ziggurat random number generator for exponentially distributed numbers
*/
// Ziggurat
static double wn[128], fn[128];
static uint32_t iz, jz, jsr = 123456789, kn[128];
static int32_t hz;
#define SHR3 (jz = jsr, jsr ^= (jsr << 13), jsr ^= (jsr >> 17), jsr ^= (jsr << 5), jz + jsr)
#define UNI (0.5 + (signed)SHR3 * 0.2328306e-9)

double nfix(void)
{
  const double r = 3.442620;
  static double x, y;

  for (;;) {
    x = hz * wn[iz];

    if (iz == 0) {
      do {
        x = -0.2904764 * log(UNI);
        y = -log(UNI);
      } while (y + y < x * x);

      return (hz > 0) ? r + x : -r - x;
    }

    if (fn[iz] + UNI * (fn[iz - 1] - fn[iz]) < exp(-0.5 * x * x)) {
      return x;
    }

    hz = SHR3;
    iz = hz & 127;

    if (abs(hz) < kn[iz]) {
      return ((hz)*wn[iz]);
    }
  }
}

/*!Procedure to create tables for normal distribution kn,wn and fn. */
static inline void tableNor(unsigned long seed)
{
  jsr = seed;
  double dn = 3.442619855899;
  int i;
  const double m1 = 2147483648.0;
  double q;
  double tn = 3.442619855899;
  const double vn = 9.91256303526217E-03;
  q = vn / exp(-0.5 * dn * dn);
  kn[0] = ((dn / q) * m1);
  kn[1] = 0;
  wn[0] = (q / m1);
  wn[127] = (dn / m1);
  fn[0] = 1.0;
  fn[127] = (exp(-0.5 * dn * dn));

  for (i = 126; 1 <= i; i--) {
    dn = sqrt(-2.0 * log(vn / dn + exp(-0.5 * dn * dn)));
    kn[i + 1] = ((dn / tn) * m1);
    tn = dn;
    fn[i] = (exp(-0.5 * dn * dn));
    wn[i] = (dn / m1);
  }
  tableNordDone=true;
  return;
}

double __attribute__ ((no_sanitize("address", "undefined"))) gaussZiggurat(double mean, double variance)
{
  if (!__builtin_expect(tableNordDone, 1)) {
    fprintf(stderr, "%s(): RNG not initialized, run randominit() first\n", __func__);
    abort();
  }

  hz = SHR3;
  iz = hz & 127;
  return hz != INT32_MIN && abs(hz) < kn[iz] ? hz * wn[iz] : nfix();
}

/*
@defgroup _gauss_noise_int16
@ingroup numerical Gaussian noise for a whole buffer of int16 samples, by a vector Box-Muller transform: no table and no
rejection, so, unlike the ziggurat, it keeps 16 values per 256-bit register.
*/
// one state per thread: 4 xorshift64 generators (period 2^64 - 1), seeded from the seed of randominit() and from the
// rank of the thread among those that draw noise
static _Thread_local uint64_t gauss_noise_state[4];
static _Thread_local bool gauss_noise_state_done = false;
static atomic_uint gauss_noise_threads;

static uint64_t splitmix64(uint64_t *x)
{
  uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

static void gauss_noise_state_init(void)
{
  uint64_t x = gauss_noise_seed + ((uint64_t)atomic_fetch_add(&gauss_noise_threads, 1) << 32);
  for (int i = 0; i < 4; i++) {
    do
      gauss_noise_state[i] = splitmix64(&x);
    while (gauss_noise_state[i] == 0); // 0 is the fixed point of a xorshift
  }
  gauss_noise_state_done = true;
}

static inline simde__m256i xorshift64x4(simde__m256i *state)
{
  simde__m256i s = *state;
  s = simde_mm256_xor_si256(s, simde_mm256_slli_epi64(s, 13));
  s = simde_mm256_xor_si256(s, simde_mm256_srli_epi64(s, 7));
  s = simde_mm256_xor_si256(s, simde_mm256_slli_epi64(s, 17));
  return *state = s;
}

/* natural logarithm of x in [2^-31, 1]: the single precision polynomial of the Cephes library (relative error 1e-7) */
static inline simde__m256 gauss_noise_log(simde__m256 x)
{
  const simde__m256i xi = simde_mm256_castps_si256(x);
  simde__m256 e = simde_mm256_cvtepi32_ps(simde_mm256_sub_epi32(simde_mm256_srli_epi32(xi, 23), simde_mm256_set1_epi32(126)));
  // mantissa in [0.5, 1), then in [sqrt(0.5) - 1, sqrt(2) - 1)
  simde__m256 m = simde_mm256_castsi256_ps(
      simde_mm256_or_si256(simde_mm256_and_si256(xi, simde_mm256_set1_epi32(0x007fffff)), simde_mm256_set1_epi32(0x3f000000)));
  const simde__m256 lt = simde_mm256_cmp_ps(m, simde_mm256_set1_ps(0.707106781186547524f), SIMDE_CMP_LT_OQ);
  e = simde_mm256_sub_ps(e, simde_mm256_and_ps(lt, simde_mm256_set1_ps(1)));
  m = simde_mm256_sub_ps(simde_mm256_add_ps(m, simde_mm256_and_ps(lt, m)), simde_mm256_set1_ps(1));
  const simde__m256 z = simde_mm256_mul_ps(m, m);
  simde__m256 y = simde_mm256_set1_ps(7.0376836292E-2f);
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(-1.1514610310E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(1.1676998740E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(-1.2420140846E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(1.4249322787E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(-1.6668057665E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(2.0000714765E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(-2.4999993993E-1f));
  y = simde_mm256_fmadd_ps(y, m, simde_mm256_set1_ps(3.3333331174E-1f));
  y = simde_mm256_mul_ps(simde_mm256_mul_ps(y, m), z);
  y = simde_mm256_fnmadd_ps(z, simde_mm256_set1_ps(0.5f), y);
  return simde_mm256_fmadd_ps(e, simde_mm256_set1_ps(0.693147180559945f), simde_mm256_add_ps(m, y));
}

/* 16 values: 8 pairs r * (cos, sin) with r = sigma * sqrt(-2 ln u1), u1 uniform in (0, 1] and a uniform angle */
static inline simde__m256i gauss_noise_16(simde__m256i *state, simde__m256 sigma)
{
  const simde__m256i a = xorshift64x4(state), b = xorshift64x4(state);
  // u1 from 31 bits: a float keeps the small values exactly, and they make the tail (up to 6.5 sigma)
  const simde__m256i a31 = simde_mm256_add_epi32(simde_mm256_srli_epi32(a, 1), simde_mm256_set1_epi32(1));
  const simde__m256 u1 = simde_mm256_mul_ps(simde_mm256_cvtepi32_ps(a31), simde_mm256_set1_ps(0x1p-31f));
  const simde__m256 r2 = simde_mm256_mul_ps(gauss_noise_log(u1), simde_mm256_set1_ps(-2));
  const simde__m256 r = simde_mm256_mul_ps(simde_mm256_sqrt_ps(r2), sigma);
  // angle t in [-pi/4, pi/4) from 29 bits, sin and cos by polynomial (Cephes); the 3 other bits choose the octant
  const simde__m256 t =
      simde_mm256_mul_ps(simde_mm256_cvtepi32_ps(simde_mm256_srai_epi32(b, 3)), simde_mm256_set1_ps(0x1p-28f * 0.785398163397448f));
  const simde__m256 t2 = simde_mm256_mul_ps(t, t);
  simde__m256 sn = simde_mm256_fmadd_ps(t2, simde_mm256_set1_ps(-1.9515295891E-4f), simde_mm256_set1_ps(8.3321608736E-3f));
  sn = simde_mm256_fmadd_ps(sn, t2, simde_mm256_set1_ps(-1.6666654611E-1f));
  sn = simde_mm256_fmadd_ps(simde_mm256_mul_ps(sn, t2), t, t);
  simde__m256 cs =
      simde_mm256_fmadd_ps(t2, simde_mm256_set1_ps(2.443315711809948E-5f), simde_mm256_set1_ps(-1.388731625493765E-3f));
  cs = simde_mm256_fmadd_ps(cs, t2, simde_mm256_set1_ps(4.166664568298827E-2f));
  const simde__m256 c1 = simde_mm256_fnmadd_ps(t2, simde_mm256_set1_ps(0.5f), simde_mm256_set1_ps(1));
  cs = simde_mm256_fmadd_ps(simde_mm256_mul_ps(cs, t2), t2, c1);
  // octant: bit 0 swaps sin and cos, bits 1 and 2 are the signs (blendv and the sign mask use the top bit)
  const simde__m256 swap = simde_mm256_castsi256_ps(simde_mm256_slli_epi32(b, 31));
  const simde__m256 sign = simde_mm256_set1_ps(-0.0f);
  simde__m256 x = simde_mm256_blendv_ps(cs, sn, swap), y = simde_mm256_blendv_ps(sn, cs, swap);
  x = simde_mm256_xor_ps(x, simde_mm256_and_ps(simde_mm256_castsi256_ps(simde_mm256_slli_epi32(b, 30)), sign));
  y = simde_mm256_xor_ps(y, simde_mm256_and_ps(simde_mm256_castsi256_ps(simde_mm256_slli_epi32(b, 29)), sign));
  // cvtps rounds to the nearest and packs saturates. packs works per 128-bit lane: the order of independent values
  // does not matter
  const simde__m256i xi = simde_mm256_cvtps_epi32(simde_mm256_mul_ps(x, r)), yi = simde_mm256_cvtps_epi32(simde_mm256_mul_ps(y, r));
  return simde_mm256_packs_epi32(xi, yi);
}

void gauss_noise_int16(int16_t *out, int n, float sigma)
{
  if (!gauss_noise_state_done)
    gauss_noise_state_init();
  // beyond the int16 range all the values would saturate, and cvtps_epi32 needs products that fit in 32 bits
  const simde__m256 vs = simde_mm256_set1_ps(sigma < 1e6f ? sigma : 1e6f);
  simde__m256i state = simde_mm256_loadu_si256((const simde__m256i *)gauss_noise_state);
  int i = 0;
  for (; i + 16 <= n; i += 16)
    simde_mm256_storeu_si256((simde__m256i *)(out + i), gauss_noise_16(&state, vs));
  if (i < n) {
    int16_t last[16];
    simde_mm256_storeu_si256((simde__m256i *)last, gauss_noise_16(&state, vs));
    memcpy(out + i, last, (n - i) * sizeof(*out));
  }
  simde_mm256_storeu_si256((simde__m256i *)gauss_noise_state, state);
}

#ifdef MAIN
main(int argc,char **argv)
{

  int i;

  randominit();

  for (i=0; i<10; i++) {
    printf("%f\n",gaussdouble(0.0,1.0));
  }
}
#endif

