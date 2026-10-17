/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"

softmodem_params_t *get_softmodem_params(void)
{
  return NULL;
}

/// one SFN cycle in subframes: the UE counts the distance to the epoch modulo this
#define SFN_CYCLE_SUBFRAMES (MAX_FRAME_NUMBER * NR_NUMBER_OF_SUBFRAMES_PER_FRAME)
#define MAX_REPORTED_FAILURES 10

typedef struct {
  int frame;
  int subframe;
  int lead_ms;
  int epoch_sfn;
  int epoch_subframe;
} epoch_case_t;

static const epoch_case_t cases[] = {
    {100, 3, 0, 100, 3}, // no lead: the current instant
    {100, 3, 2500, 350, 3}, // lead within the SFN cycle
    {5, 9, 1, 6, 0}, // the subframe carries into the frame
    {1023, 9, 1, 0, 0}, // the SFN wraps
    {1000, 0, 2500, 226, 0}, // the SFN wraps under a long lead
    {0, 0, NR_SIB19_MAX_EPOCH_LEAD_MS, 1023, 9}, // the longest lead
};

static int failures = 0;

static void report(int frame, int subframe, int lead_ms, int sfn, int sf, const char *expected)
{
  if (++failures <= MAX_REPORTED_FAILURES)
    printf("FAIL: %d.%d + %d ms gives epoch %d.%d, expected %s\n", frame, subframe, lead_ms, sfn, sf, expected);
}

int main(void)
{
  for (int i = 0; i < sizeofArray(cases); i++) {
    const epoch_case_t *c = &cases[i];
    int sfn = -1, sf = -1;
    nr_sib19_epoch_from_lead(c->frame, c->subframe, c->lead_ms, &sfn, &sf);
    if (sfn != c->epoch_sfn || sf != c->epoch_subframe) {
      char expected[32];
      snprintf(expected, sizeof(expected), "%d.%d", c->epoch_sfn, c->epoch_subframe);
      report(c->frame, c->subframe, c->lead_ms, sfn, sf, expected);
    }
  }

  // from every current time, the epoch must lie exactly lead_ms ahead, counted the way the UE does
  const int leads[] = {0, 1, 9, 10, 2500, NR_SIB19_MAX_EPOCH_LEAD_MS};
  for (int frame = 0; frame < MAX_FRAME_NUMBER; frame++) {
    for (int subframe = 0; subframe < NR_NUMBER_OF_SUBFRAMES_PER_FRAME; subframe++) {
      for (int l = 0; l < sizeofArray(leads); l++) {
        int sfn = -1, sf = -1;
        nr_sib19_epoch_from_lead(frame, subframe, leads[l], &sfn, &sf);
        const bool in_range = sfn >= 0 && sfn < MAX_FRAME_NUMBER && sf >= 0 && sf < NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
        const int now = frame * NR_NUMBER_OF_SUBFRAMES_PER_FRAME + subframe;
        const int epoch = sfn * NR_NUMBER_OF_SUBFRAMES_PER_FRAME + sf;
        if (!in_range || (epoch - now + SFN_CYCLE_SUBFRAMES) % SFN_CYCLE_SUBFRAMES != leads[l])
          report(frame, subframe, leads[l], sfn, sf, "the lead ahead, within one SFN cycle");
      }
    }
  }

  if (failures)
    printf("%d failures\n", failures);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
