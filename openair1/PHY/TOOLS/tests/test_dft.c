#include <stdio.h>
#include <math.h>
#include "openair1/PHY/TOOLS/tools_defs.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "common/utils/utils.h"

// should be FOREACH_DFTSZ but we restrict to tested DFT sizes
#define FOREACH_DFTSZ_working(SZ_DEF) \
  SZ_DEF(12)                          \
  SZ_DEF(16)                          \
  SZ_DEF(64)                          \
  SZ_DEF(128)                         \
  SZ_DEF(256)                         \
  SZ_DEF(512)                         \
  SZ_DEF(768)                         \
  SZ_DEF(1024)                        \
  SZ_DEF(1536)                        \
  SZ_DEF(2048)                        \
  SZ_DEF(3072)                        \
  SZ_DEF(4096)                        \
  SZ_DEF(6144)                        \
  SZ_DEF(8192)                        \
  SZ_DEF(12288)

#define SZ_PTR(Sz) {Sz},
struct {
  int size;
} const dftFtab[] = {FOREACH_DFTSZ_working(SZ_PTR)};

#define SZ_iPTR(Sz) {Sz},
struct {
  int size;
} const idftFtab[] = {FOREACH_IDFTSZ(SZ_iPTR)};

bool error(c16_t v16, cd_t vd, double percent)
{
  cd_t err = {abs(v16.r - vd.r), abs(v16.i - vd.i)};
  if (err.r < 10 && err.i < 10)
    return false; // ignore quantization noise
  int denomr = min(abs(v16.r), abs(vd.r));
  if (denomr && err.r / denomr > percent / 100)
    return true;
  int denomi = min(abs(v16.i), abs(vd.i));
  if (denomi && err.i / denomi > percent / 100)
    return true;
  return false;
}

void math_dft(c16_t *in, cd_t *out, int len, int dir, int norm)
{
  for (int k = 0; k < len; k++) {
    cd_t tmp = {0};
    // wrote this way to help gcc to generate SIMD
    double phi[len], sint[len], cost[len];
    for (int n = 0; n < len; n++)
      if (dir == 0)
        phi[n] = -2 * M_PI * ((double)k / len) * n;
      else
        phi[n] = 2 * M_PI * ((double)k / len) * n;
    for (int n = 0; n < len; n++)
      sint[n] = sin(phi[n]);
    for (int n = 0; n < len; n++)
      cost[n] = cos(phi[n]);
    for (int n = 0; n < len; n++) {
      cd_t coeff = {.r = cost[n], .i = sint[n]};
      cd_t in16q = {.r = (double)in[n].r, .i = (double)in[n].i};
      cd_t component = cdMul(coeff, in16q);
      tmp.r += component.r;
      tmp.i += component.i;
    }
    out[k].r = tmp.r / ((norm == 0) ? 1.0 : sqrt(len));
    out[k].i = tmp.i / ((norm == 0) ? 1.0 : sqrt(len));
  }
}

void fill_qam(int n, cd_t *x, int mod)
{
  int size;
  if (mod < 0 || mod > 1) {
    printf("Illegal modulation %d\n", mod);
    exit(-1);
  }
  double sqrt170 = 1.0 / sqrt(170);
  memset((void *)&x[0], 0, n * sizeof(cd_t));
  switch (n) {
    case 128:
      size = 72;
      break;
    case 256:
      size = 180;
      break;
    case 512:
      size = 300;
      break;
    case 768:
      size = 612;
      break;
    case 1024:
      size = 612;
      break;
    case 1536:
      size = 900;
      break;
    case 2048:
      size = 1596;
      break;
    case 3072:
      size = 2556;
      break;
    case 4096:
      size = 3276;
      break;
    default:
      printf("Illegal FFT length %d\n", n);
      exit(-1);
      ;
  }
  for (int i = 0; i < size / 2; i++) {
    if (mod == 0) {
      int rv = taus() & 1;
      x[i].r = (1 / sqrt(2.0)) * ((rv << 1) - 1);
      rv = taus() & 1;
      x[i].i = (1 / sqrt(2.0)) * ((rv << 1) - 1);
    } else {
      int rvi = taus() & 15;
      int rvq = taus() & 15;
      x[i].r = ((1 - 2 * (rvi & 1))
                * (8 - (1 - 2 * ((rvi >> 1) & 1)) * (4 - (1 - 2 * ((rvi >> 2) & 1)) * (2 - (1 - 2 * ((rvi >> 3) & 1))))))
               * sqrt170;
      x[i].i = ((1 - 2 * (rvq & 1))
                * (8 - (1 - 2 * ((rvq >> 1) & 1)) * (4 - (1 - 2 * ((rvq >> 2) & 1)) * (2 - (1 - 2 * ((rvq >> 3) & 1))))))
               * sqrt170;
    }
  }
  for (int i = n - (size / 2); i < n; i++) {
    if (mod == 0) {
      int rv = taus() & 1;
      x[i].r = (1 / sqrt(2.0)) * ((rv << 1) - 1);
      rv = taus() & 1;
      x[i].i = (1 / sqrt(2.0)) * ((rv << 1) - 1);
    } else {
      int rvi = taus() & 15;
      int rvq = taus() & 15;
      x[i].r = ((1 - 2 * (rvi & 1))
                * (8 - (1 - 2 * ((rvi >> 1) & 1)) * (4 - (1 - 2 * ((rvi >> 2) & 1)) * (2 - (1 - 2 * ((rvi >> 3) & 1))))))
               * sqrt170;
      x[i].i = ((1 - 2 * (rvq & 1))
                * (8 - (1 - 2 * ((rvq >> 1) & 1)) * (4 - (1 - 2 * ((rvq >> 2) & 1)) * (2 - (1 - 2 * ((rvq >> 3) & 1))))))
               * sqrt170;
    }
  }
}

// #define RUN_SIMULATION
#ifdef RUN_SIMULATION
#define NUM_TRIALS 10000
#else
#define NUM_TRIALS 1
#endif

int main(void)
{
  randominit();
  int n_simu = 4096;
  int ret = 0;
  load_dftslib();
  c16_t *d16 = malloc16(12 * dftFtab[sizeofArray(dftFtab) - 1].size * sizeof(*d16));
  c16_t *d16_2 = malloc16(12 * sizeof(*d16_2));
  c16_t *o16 = malloc16(12 * dftFtab[sizeofArray(dftFtab) - 1].size * sizeof(*d16));
  set_taus_seed(0);
  for (int sz = 0; sz < sizeofArray(dftFtab); sz++) {
    const int n = dftFtab[sz].size;
#if RUN_SIMULATION
    if (n != n_simu)
      continue;
#endif

    double coeffs[] = {30, 40, 50, 60, 70};
    double evm[sizeofArray(coeffs)] = {0};
    double sqnr[sizeofArray(coeffs)] = {0};
    double samples[sizeofArray(coeffs)] = {0};
    double samples_out[sizeofArray(coeffs)] = {0};
    double mse[sizeofArray(coeffs)] = {0};
    double error_vector[sizeofArray(coeffs)][n];
    const int num_coeffs = sizeofArray(coeffs);
    double *error_vector_d = malloc(num_coeffs * NUM_TRIALS * n * sizeof(*error_vector_d));
    memset(error_vector, 0, sizeof(error_vector));
    memset(error_vector_d, 0, num_coeffs * NUM_TRIALS * n * sizeof(*error_vector_d));
    double (*error_vec_d_p)[num_coeffs][NUM_TRIALS][n] = (double (*)[num_coeffs][NUM_TRIALS][n])error_vector_d;
    printf("Testing size %d\n", n);
    for (int t = 0; t < NUM_TRIALS; t++) {
      cd_t data[n];
      cd_t out[n];
      for (int i = 0; i < n; i++) {
        data[i].r = gaussZiggurat(0, 1.0); // gaussZiggurat not used paramters, to fix
        data[i].i = gaussZiggurat(0, 1.0);
      }
      for (int coeff = 0; coeff < sizeofArray(coeffs); coeff++) {
        double expand = pow(10.0, .05 * coeffs[coeff]) / sqrt(2);
        if (n == 12) {
          for (int i = 0; i < n; i++) {
            for (int j = 0; j < 4; j++) {
              d16[i * 4 + j].r = expand * data[i].r;
              d16[i * 4 + j].i = expand * data[i].i;
            }
            d16_2[i].r = d16[i * 4].r;
            d16_2[i].i = d16[i * 4].i;
          }
        } else {
          for (int i = 0; i < n; i++) {
            d16[i].r = expand * data[i].r;
            d16[i].i = expand * data[i].i;
          }
        }
        if (n == 12)
          math_dft(d16_2, out, n, 0, 0);
        else
          math_dft(d16, out, n, 0, 1);
        dft(get_dft(n), (int16_t *)d16, (int16_t *)o16, 0);
        double mse_trial = 0;
        double samples_trial = 0;
        double samples_out_trial = 0;
        double sqnr_trial = 0;
        double evm_trial = 0;
        if (n == 12) {
          for (int i = 0; i < n; i++) {
            cd_t error = {.r = o16[i * 4].r - out[i].r, .i = o16[i * 4].i - out[i].i};
            error_vector[coeff][i] += squaredMod(error);
            sqnr_trial += squaredMod(error);
            evm_trial += sqrt(squaredMod(error)) / sqrt(squaredMod(out[i]));
            samples_out_trial += (squaredMod(out[i]));
            samples_trial += squaredMod(d16_2[i]);
          }
        } else {
          for (int i = 0; i < n; i++) {
            // if (n==n_simu) printf("i: %d, o: %f,%f, dft: %d,%d\n",i,out[i].r,out[i].i,o16[i].r,o16[i].i);
            cd_t error = {.r = o16[i].r - out[i].r, .i = o16[i].i - out[i].i};
            evm_trial += sqrt(squaredMod(error)) / sqrt(squaredMod(out[i]));
            double error_dB = 10 * log10(squaredMod(error));
            error_vector[coeff][i] += squaredMod(error);
            (*error_vec_d_p)[coeff][t][i] = squaredMod(error);
            if (coeffs[coeff] == 50 && n == n_simu && NUM_TRIALS == 1 && error_dB >= 10)
              printf("error in DFT pos %d : in %f dB, error %f (%f dB) \n", i, coeffs[coeff], squaredMod(error), error_dB);
            sqnr_trial += squaredMod(error);
            samples_trial += squaredMod(d16[i]);
            samples_out_trial += squaredMod(out[i]);
            mse_trial += squaredMod(error) * squaredMod(error);
          }
        }
        samples[coeff] = samples_trial;
        samples_out[coeff] = samples_out_trial;
        sqnr[coeff] += (samples_out_trial / sqnr_trial);
        mse[coeff] += (mse_trial / n);
        evm[coeff] += ((evm_trial / n) * 100);
      }
      fflush(stdout);
      if (NUM_TRIALS == 1) {
        printf("done DFT size %d (evm (%%), SQNRdB, avg in samples amplitude, avg out samples amplitude) = \n", n);
        for (int coeff = 0; coeff < sizeofArray(coeffs); coeff++)
          printf("input_lev %f (%.2f, %f, %.1f, %.1f)\n",
                 coeffs[coeff],
                 evm[coeff],
                 10 * log10(sqnr[coeff]),
                 10 * log10(samples[coeff] / n),
                 10 * log10(samples_out[coeff] / n));
        printf("\n");
        int i;
        for (i = 0; i < sizeofArray(coeffs); i++)
          if (evm[i] / n < 0.01)
            break;
        if (i == sizeofArray(coeffs)) {
          printf("DFT size: %d, minimum error is more than 1%%, setting the test as failed\n", n);
          ret = 1;
        }
      }
    }
    for (int c = 0; c < sizeofArray(coeffs); c++) {
      mse[c] /= NUM_TRIALS;
      sqnr[c] /= NUM_TRIALS;
      evm[c] /= NUM_TRIALS;
      for (int i = 0; i < n; i++) {
        error_vector[c][i] /= NUM_TRIALS;
      }
    }

#if RUN_SIMULATION
    printf("writing results to file\n");
    FILE *file = fopen("dft_results.csv", "w+");
    for (int c = 0; c < sizeofArray(coeffs); c++) {
      fprintf(file, "%f,", coeffs[c]);
      printf("MSE, SQNR, EVM for %.2f: (%f, %f, %f)\n", coeffs[c], mse[c], 10 * log10(sqnr[c]), evm[c]);
    }
    fprintf(file, "\n");

    for (int i = 0; i < n; i++) {
      for (int c = 0; c < sizeofArray(coeffs); c++) {
        fprintf(file, "%4.4f,", error_vector[c][i]);
      }
      fprintf(file, "\n");
    }
    fclose(file);

    file = fopen("error_dist.csv", "w+");
    for (int p = 0; p < n; p++) {
      fprintf(file, "%d,", p);
    }
    fprintf(file, "\n");

    for (int i = 0; i < NUM_TRIALS; i++) {
      for (int p = 0; p < n; p++) {
        fprintf(file, "%4.4f,", (*error_vec_d_p)[0][i][p]);
      }
      fprintf(file, "\n");
    }
    fclose(file);
    free(error_vector_d);
  }
#else
    free(error_vector_d);
  }

  // TX test: modulate all used sizss with QPSK and 256QAM. Compute IDFT using
  // QAM levels and IDFT scaling used in gNB transmit chain. Use double precision DFT
  // to bring back to frequency-domain. Compute EVM and SQNR compared to
  // transmitted waveform
  for (int sz = 0; sz < sizeofArray(dftFtab); sz++) {
    const int n = dftFtab[sz].size;
    cd_t data[n];
    if (n > 4096)
      break;
    if (n < 128)
      continue;
    printf("Testing IDFT size %d\n", n);
    cd_t out[n];
    for (int mod = 0; mod < 2; mod++) {
      fill_qam(n, data, mod);
      int16_t amp = 512;
      for (int i = 0; i < n; i++) {
        d16[i].r = (int16_t)(amp * data[i].r);
        d16[i].i = (int16_t)(amp * data[i].i);
      }
      idft(get_idft(n), (int16_t *)d16, (int16_t *)o16, 0);
      math_dft(o16, out, n, 0, 1);
      double evm = 0;
      double sqnr = 0;
      double samples = 0;
      double samples_out = 0;
      int nz = 0;
      for (int i = 0; i < n; i++) {
        // if (n==128) printf("i: %d, o: %f,%f, dft: %d,%d\n",i,out[i].r,out[i].i,d16[i].r,d16[i].i);
        if (data[i].r != 0) {
          cd_t error = {.r = (double)d16[i].r - out[i].r, .i = (double)d16[i].i - out[i].i};
          evm += sqrt(squaredMod(error)) / sqrt(squaredMod(out[i]));
          sqnr += squaredMod(error);
          samples += sqrt(squaredMod(d16[i]));
          samples_out += squaredMod(out[i]);
          nz++;
        }
      }
      sqnr = samples_out / sqnr;
      printf("done IDFT size %d nz %d mod %s (evm (%%), SQNRdB, avg samples amplitude) = ", n, nz, mod == 0 ? "QPSK" : "256QAM");
      printf("(%.2f, %f, %.1f) ", (evm / nz) * 100, 10 * log10(sqnr), 10 * log10(samples_out / nz));
      printf("\n");
      if (evm / nz > 0.01) {
        printf("IDFT size: %d/ mod %s, minimum error is more than 1%%, setting the test as failed\n",
               n,
               mod == 0 ? "QPSK" : "256QAM");
        ret = 1;
        break;
      }
    }
    fflush(stdout);
  }
#endif
  free(d16);
  free(o16);
  free(d16_2);
  return ret;
}
