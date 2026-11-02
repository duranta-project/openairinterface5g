/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include  "sim.h"
#include "gaussian_rng.h"

static unsigned int urseed, iy, ir[98]; /// uniformrandom
static bool gaussian_initialized = false; /// gaussZiggurat
static gaussian_rng_t default_gaussian;

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
  gaussian_rng_init(seed, &default_gaussian);
  gaussian_initialized = true;
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

static inline gaussian_rng_t *default_rng(void)
{
  if (!__builtin_expect(gaussian_initialized, 1)) {
    fprintf(stderr, "%s(): RNG not initialized, run randominit() first\n", __func__);
    abort();
  }
  return &default_gaussian;
}

gaussian_rng_t *gaussian_rng_default(void)
{
  return default_rng();
}

double gaussZiggurat(double mean, double variance)
{
  /* The legacy signature's parameters have always been ignored. */
  (void)mean;
  (void)variance;
  return gaussian_rng_sample(default_rng());
}

/* Keep compatibility wrappers beside their shared state to avoid a lookup call. */
void add_noise_float(c16_t **rxdata,
                     const float **r_re,
                     const float **r_im,
                     const float sigma,
                     const int length,
                     const int slot_offset,
                     const double ts,
                     const int delay,
                     bool apply_phase_noise,
                     const uint8_t nb_antennas_rx)
{
  if (length <= 0 || nb_antennas_rx == 0)
    return;
  add_noise_float_batch(rxdata,
                        r_re,
                        r_im,
                        sigma,
                        length,
                        slot_offset,
                        ts,
                        delay,
                        apply_phase_noise,
                        nb_antennas_rx,
                        default_rng());
}

#ifdef MAIN
main(int argc, char **argv)
{

  int i;

  randominit();

  for (i=0; i<10; i++) {
    printf("%f\n",gaussdouble(0.0,1.0));
  }
}
#endif

