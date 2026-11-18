/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "assertions.h"
#include "cdl_channel.h"

typedef struct {
  double delay; ///< normalized
  double power_db;
  double aod, aoa, zod, zoa; ///< degrees
} cdl_cluster_t;

typedef struct {
  const cdl_cluster_t *clusters;
  int num_clusters;
  bool los; ///< clusters[0] is the specular LOS ray
  double c_asd, c_asa, c_zsd, c_zsa; ///< cluster angle spreads, degrees
} cdl_table_t;

// clang-format off
// TR 38.901 Table 7.7.1-1
static const cdl_cluster_t cdl_a_clusters[] = {
    {0.0, -13.4, -178.1, 51.3, 50.2, 125.4},
    {0.3819, 0.0, -4.2, -152.7, 93.2, 91.3},
    {0.4025, -2.2, -4.2, -152.7, 93.2, 91.3},
    {0.5868, -4.0, -4.2, -152.7, 93.2, 91.3},
    {0.461, -6.0, 90.2, 76.6, 122.0, 94.0},
    {0.5375, -8.2, 90.2, 76.6, 122.0, 94.0},
    {0.6708, -9.9, 90.2, 76.6, 122.0, 94.0},
    {0.575, -10.5, 121.5, -1.8, 150.2, 47.1},
    {0.7618, -7.5, -81.7, -41.9, 55.2, 56.0},
    {1.5375, -15.9, 158.4, 94.2, 26.4, 30.1},
    {1.8978, -6.6, -83.0, 51.9, 126.4, 58.8},
    {2.2242, -16.7, 134.8, -115.9, 171.6, 26.0},
    {2.1718, -12.4, -153.0, 26.6, 151.4, 49.2},
    {2.4942, -15.2, -172.0, 76.6, 157.2, 143.1},
    {2.5119, -10.8, -129.9, -7.0, 47.2, 117.4},
    {3.0582, -11.3, -136.0, -23.0, 40.4, 122.7},
    {4.081, -12.7, 165.4, -47.2, 43.3, 123.2},
    {4.4579, -16.2, 148.4, 110.4, 161.8, 32.6},
    {4.5695, -18.3, 132.7, 144.5, 10.8, 27.2},
    {4.7966, -18.9, -118.6, 155.3, 16.7, 15.2},
    {5.0066, -16.6, -154.1, 102.0, 171.7, 146.0},
    {5.3043, -19.9, 126.5, -151.8, 22.7, 150.7},
    {9.6586, -29.7, -56.2, 55.2, 144.9, 156.1},
};
// TR 38.901 Table 7.7.1-2
static const cdl_cluster_t cdl_b_clusters[] = {
    {0.0, 0.0, 9.3, -173.3, 105.8, 78.9},
    {0.1072, -2.2, 9.3, -173.3, 105.8, 78.9},
    {0.2155, -4.0, 9.3, -173.3, 105.8, 78.9},
    {0.2095, -3.2, -34.1, 125.5, 115.3, 63.3},
    {0.287, -9.8, -65.4, -88.0, 119.3, 59.9},
    {0.2986, -1.2, -11.4, 155.1, 103.2, 67.5},
    {0.3752, -3.4, -11.4, 155.1, 103.2, 67.5},
    {0.5055, -5.2, -11.4, 155.1, 103.2, 67.5},
    {0.3681, -7.6, -67.2, -89.8, 118.2, 82.6},
    {0.3697, -3.0, 52.5, 132.1, 102.0, 66.3},
    {0.57, -8.9, -72.0, -83.6, 100.4, 61.6},
    {0.5283, -9.0, 74.3, 95.3, 98.3, 58.0},
    {1.1021, -4.8, -52.2, 103.7, 103.4, 78.2},
    {1.2756, -5.7, -50.5, -87.8, 102.5, 82.0},
    {1.5474, -7.5, 61.4, -92.5, 101.4, 62.4},
    {1.7842, -1.9, 30.6, -139.1, 103.0, 78.0},
    {2.0169, -7.6, -72.5, -90.6, 100.0, 60.9},
    {2.8294, -12.2, -90.6, 58.6, 115.2, 82.9},
    {3.0219, -9.8, -77.6, -79.0, 100.5, 60.8},
    {3.6187, -11.4, -82.6, 65.8, 119.6, 57.3},
    {4.1067, -14.9, -103.6, 52.7, 118.7, 59.9},
    {4.279, -9.2, 75.6, 88.7, 117.8, 60.1},
    {4.7834, -11.3, -77.6, -60.4, 115.7, 62.3},
};
// TR 38.901 Table 7.7.1-3
static const cdl_cluster_t cdl_c_clusters[] = {
    {0.0, -4.4, -46.6, -101.0, 97.2, 87.6},
    {0.2099, -1.2, -22.8, 120.0, 98.6, 72.1},
    {0.2219, -3.5, -22.8, 120.0, 98.6, 72.1},
    {0.2329, -5.2, -22.8, 120.0, 98.6, 72.1},
    {0.2176, -2.5, -40.7, -127.5, 100.6, 70.1},
    {0.6366, 0.0, 0.3, 170.4, 99.2, 75.3},
    {0.6448, -2.2, 0.3, 170.4, 99.2, 75.3},
    {0.656, -3.9, 0.3, 170.4, 99.2, 75.3},
    {0.6584, -7.4, 73.1, 55.4, 105.2, 67.4},
    {0.7935, -7.1, -64.5, 66.5, 95.3, 63.8},
    {0.8213, -10.7, 80.2, -48.1, 106.1, 71.4},
    {0.9336, -11.1, -97.1, 46.9, 93.5, 60.5},
    {1.2285, -5.1, -55.3, 68.1, 103.7, 90.6},
    {1.3083, -6.8, -64.3, -68.7, 104.2, 60.1},
    {2.1704, -8.7, -78.5, 81.5, 93.0, 61.0},
    {2.7105, -13.2, 102.7, 30.7, 104.2, 100.7},
    {4.2589, -13.9, 99.2, -16.4, 94.9, 62.3},
    {4.6003, -13.9, 88.8, 3.8, 93.1, 66.7},
    {5.4902, -15.8, -101.9, -13.7, 92.2, 52.9},
    {5.6077, -17.1, 92.2, 9.7, 106.7, 61.8},
    {6.3065, -16.0, 93.3, 5.6, 93.0, 51.9},
    {6.6374, -15.7, 106.6, 0.7, 92.9, 61.7},
    {7.0427, -21.6, 119.5, -21.9, 105.2, 58.0},
    {8.6523, -22.8, -123.8, 33.6, 107.8, 57.0},
};
// TR 38.901 Table 7.7.1-4 (row 0: specular LOS ray)
static const cdl_cluster_t cdl_d_clusters[] = {
    {0.0, -0.2, 0.0, -180.0, 98.5, 81.5},
    {0.0, -13.5, 0.0, -180.0, 98.5, 81.5},
    {0.035, -18.8, 89.2, 89.2, 85.5, 86.9},
    {0.612, -21.0, 89.2, 89.2, 85.5, 86.9},
    {1.363, -22.8, 89.2, 89.2, 85.5, 86.9},
    {1.405, -17.9, 13.0, 163.0, 97.5, 79.4},
    {1.804, -20.1, 13.0, 163.0, 97.5, 79.4},
    {2.596, -21.9, 13.0, 163.0, 97.5, 79.4},
    {1.775, -22.9, 34.6, -137.0, 98.5, 78.2},
    {4.042, -27.8, -64.5, 74.5, 88.4, 73.6},
    {7.937, -23.6, -32.9, 127.7, 91.3, 78.3},
    {9.424, -24.8, 52.6, -119.6, 103.8, 87.0},
    {9.708, -30.0, -132.1, -9.1, 80.3, 70.6},
    {12.525, -27.7, 77.2, -83.8, 86.5, 72.9},
};
// TR 38.901 Table 7.7.1-5 (row 0: specular LOS ray)
static const cdl_cluster_t cdl_e_clusters[] = {
    {0.0, -0.03, 0.0, -180.0, 99.6, 80.4},
    {0.0, -22.03, 0.0, -180.0, 99.6, 80.4},
    {0.5133, -15.8, 57.5, 18.2, 104.2, 80.4},
    {0.544, -18.1, 57.5, 18.2, 104.2, 80.4},
    {0.563, -19.8, 57.5, 18.2, 104.2, 80.4},
    {0.544, -22.9, -20.1, 101.8, 99.4, 80.8},
    {0.7112, -22.4, 16.2, 112.9, 100.8, 86.3},
    {1.9092, -18.6, 9.3, -155.5, 98.8, 82.7},
    {1.9293, -20.8, 9.3, -155.5, 98.8, 82.7},
    {1.9589, -22.6, 9.3, -155.5, 98.8, 82.7},
    {2.6426, -22.3, 19.0, -143.3, 100.8, 82.9},
    {3.7136, -25.6, 32.7, -94.7, 96.4, 88.0},
    {5.4524, -20.2, 0.5, 147.0, 98.9, 81.0},
    {12.0034, -29.8, 55.9, -36.2, 95.6, 88.6},
    {20.6419, -29.2, 57.6, -26.0, 104.6, 78.3},
};
static const cdl_table_t cdl_tables[] = {
    [CDL_A] = {cdl_a_clusters, sizeof(cdl_a_clusters) / sizeof(cdl_cluster_t), false, 5.0, 11.0, 3.0, 3.0},
    [CDL_B] = {cdl_b_clusters, sizeof(cdl_b_clusters) / sizeof(cdl_cluster_t), false, 10.0, 22.0, 3.0, 7.0},
    [CDL_C] = {cdl_c_clusters, sizeof(cdl_c_clusters) / sizeof(cdl_cluster_t), false, 2.0, 15.0, 3.0, 7.0},
    [CDL_D] = {cdl_d_clusters, sizeof(cdl_d_clusters) / sizeof(cdl_cluster_t), true, 5.0, 8.0, 3.0, 3.0},
    [CDL_E] = {cdl_e_clusters, sizeof(cdl_e_clusters) / sizeof(cdl_cluster_t), true, 5.0, 11.0, 3.0, 7.0},
};
// clang-format on

// TR 38.901 Table 7.5-3
static const double ray_offsets[] = {0.0447, -0.0447, 0.1413, -0.1413, 0.2492, -0.2492, 0.3715, -0.3715, 0.5129, -0.5129,
                                     0.6797, -0.6797, 0.8844, -0.8844, 1.1481, -1.1481, 1.5195, -1.5195, 2.1551, -2.1551};
#define RAYS_PER_CLUSTER (int)(sizeof(ray_offsets) / sizeof(ray_offsets[0]))

typedef struct {
  double amplitude;
  double phase;
  double aod, zod; ///< model angles, before translation
} ray_t;

struct cdl_channel {
  int num_rays;
  ray_t *rays;
  channel_path_t *paths;
  double mean_aod, mean_zod; ///< of the model
  double wavelength;
  uint64_t rng;
};

static uint64_t splitmix64(uint64_t *state)
{
  uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static double uniform(uint64_t *state)
{
  return (splitmix64(state) >> 11) * 0x1.0p-53;
}

static void shuffle(double *v, int n, uint64_t *state)
{
  for (int i = n - 1; i > 0; i--) {
    const int j = splitmix64(state) % (i + 1);
    const double t = v[i];
    v[i] = v[j];
    v[j] = t;
  }
}

static double rad(double deg)
{
  return deg * M_PI / 180;
}

// Zenith outside [0, pi] continues over the pole: TR 38.901 eq. 7.5-16
static void wrap_zenith(double *zenith, double *azimuth)
{
  if (*zenith < 0) {
    *zenith = -*zenith;
    *azimuth += M_PI;
  } else if (*zenith > M_PI) {
    *zenith = 2 * M_PI - *zenith;
    *azimuth += M_PI;
  }
}

cdl_channel_t *cdl_channel_create(const cdl_channel_config_t *cfg)
{
  AssertFatal(cfg->model >= CDL_A && cfg->model <= CDL_E, "invalid CDL model %d\n", cfg->model);
  AssertFatal(cfg->carrier_hz > 0, "invalid carrier %f\n", cfg->carrier_hz);
  const cdl_table_t *t = &cdl_tables[cfg->model];
  cdl_channel_t *ch = calloc(1, sizeof(*ch));
  AssertFatal(ch, "out of memory\n");
  ch->rng = cfg->seed;
  ch->wavelength = 299792458.0 / cfg->carrier_hz;
  ch->num_rays = t->num_clusters * RAYS_PER_CLUSTER - (t->los ? RAYS_PER_CLUSTER - 1 : 0);
  ch->rays = calloc(ch->num_rays, sizeof(*ch->rays));
  ch->paths = calloc(ch->num_rays, sizeof(*ch->paths));
  AssertFatal(ch->rays && ch->paths, "out of memory\n");

  double total = 0;
  for (int n = 0; n < t->num_clusters; n++)
    total += pow(10, t->clusters[n].power_db / 10);

  // Model mean angles, TR 38.901 §7.7.5.1: power-weighted circular mean (azimuth), mean (zenith)
  double sin_sum = 0, cos_sum = 0, zod_sum = 0;
  int i = 0;
  for (int n = 0; n < t->num_clusters; n++) {
    const cdl_cluster_t *c = &t->clusters[n];
    const double power = pow(10, c->power_db / 10) / total;
    sin_sum += power * sin(rad(c->aod));
    cos_sum += power * cos(rad(c->aod));
    zod_sum += power * c->zod;
    const bool specular = t->los && n == 0;
    const int nrays = specular ? 1 : RAYS_PER_CLUSTER;
    double aod[RAYS_PER_CLUSTER], aoa[RAYS_PER_CLUSTER], zod[RAYS_PER_CLUSTER], zoa[RAYS_PER_CLUSTER];
    for (int m = 0; m < nrays; m++) {
      const double offset = specular ? 0 : ray_offsets[m];
      aod[m] = c->aod + t->c_asd * offset;
      aoa[m] = c->aoa + t->c_asa * offset;
      zod[m] = c->zod + t->c_zsd * offset;
      zoa[m] = c->zoa + t->c_zsa * offset;
    }
    // Random coupling of the rays within the cluster (§7.7.1 step 2)
    shuffle(aoa, nrays, &ch->rng);
    shuffle(zod, nrays, &ch->rng);
    shuffle(zoa, nrays, &ch->rng);
    for (int m = 0; m < nrays; m++, i++) {
      ray_t *r = &ch->rays[i];
      r->amplitude = sqrt(power / nrays);
      // Random initial phase (§7.7.1 step 4); the specular ray keeps phase 0
      r->phase = specular ? 0 : 2 * M_PI * uniform(&ch->rng) - M_PI;
      r->aod = rad(aod[m]);
      r->zod = rad(zod[m]);
      channel_path_t *p = &ch->paths[i];
      p->delay_s = c->delay * cfg->delay_spread_s;
      p->aoa = rad(aoa[m]);
      p->zoa = rad(zoa[m]);
      wrap_zenith(&p->zoa, &p->aoa);
      p->gain = (cd_t){r->amplitude * cos(r->phase), r->amplitude * sin(r->phase)};
    }
  }
  DevAssert(i == ch->num_rays);
  ch->mean_aod = atan2(sin_sum, cos_sum);
  ch->mean_zod = rad(zod_sum);
  cdl_channel_set_geometry(ch, ch->mean_aod, ch->mean_zod, (const double[3]){0, 0, 0});
  return ch;
}

void cdl_channel_free(cdl_channel_t *ch)
{
  if (!ch)
    return;
  free(ch->rays);
  free(ch->paths);
  free(ch);
}

void cdl_channel_set_geometry(cdl_channel_t *ch, double aod, double zod, const double velocity[3])
{
  for (int i = 0; i < ch->num_rays; i++) {
    const ray_t *r = &ch->rays[i];
    channel_path_t *p = &ch->paths[i];
    p->aod = r->aod - ch->mean_aod + aod;
    p->zod = r->zod - ch->mean_zod + zod;
    wrap_zenith(&p->zod, &p->aod);
    p->aod = remainder(p->aod, 2 * M_PI);
    // TR 38.901 eq. 7.5-25: Doppler of the arrival direction
    const double rx_dir[3] = {sin(p->zoa) * cos(p->aoa), sin(p->zoa) * sin(p->aoa), cos(p->zoa)};
    p->doppler_hz = (rx_dir[0] * velocity[0] + rx_dir[1] * velocity[1] + rx_dir[2] * velocity[2]) / ch->wavelength;
  }
}

void cdl_channel_advance(cdl_channel_t *ch, double dt_s)
{
  for (int i = 0; i < ch->num_rays; i++) {
    ray_t *r = &ch->rays[i];
    channel_path_t *p = &ch->paths[i];
    r->phase = remainder(r->phase + 2 * M_PI * p->doppler_hz * dt_s, 2 * M_PI);
    p->gain = (cd_t){r->amplitude * cos(r->phase), r->amplitude * sin(r->phase)};
  }
}

int cdl_channel_paths(const cdl_channel_t *ch, const channel_path_t **paths)
{
  *paths = ch->paths;
  return ch->num_rays;
}
