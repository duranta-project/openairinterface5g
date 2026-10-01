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

#define MAX_REPORTED_FAILURES 10

/// lead of an epoch over the current frame and subframe, as the UE counts it for a serving cell (configure_ntn_ta()):
/// an epoch SFN at or after the current SFN lies in this SFN cycle, an earlier one in the next
static int ue_lead_ms(int frame, int subframe, int epoch_sfn, int epoch_subframe)
{
  const int cycles = epoch_sfn >= frame ? 0 : 1;
  return (cycles * MAX_FRAME_NUMBER + epoch_sfn - frame) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME + epoch_subframe - subframe;
}

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
    {0, 0, NR_SIB19_MAX_EPOCH_LEAD_MS, 1023, 0}, // the longest lead
    {5, 9, NR_SIB19_MAX_EPOCH_LEAD_MS, 4, 9}, // the longest lead ends in the frame before the current one
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

  // from every current time and for every lead, the epoch must lie exactly the lead ahead, counted the way the UE does
  for (int frame = 0; frame < MAX_FRAME_NUMBER; frame++) {
    for (int subframe = 0; subframe < NR_NUMBER_OF_SUBFRAMES_PER_FRAME; subframe++) {
      for (int lead = 0; lead <= NR_SIB19_MAX_EPOCH_LEAD_MS; lead++) {
        int sfn = -1, sf = -1;
        nr_sib19_epoch_from_lead(frame, subframe, lead, &sfn, &sf);
        const bool in_range = sfn >= 0 && sfn < MAX_FRAME_NUMBER && sf >= 0 && sf < NR_NUMBER_OF_SUBFRAMES_PER_FRAME;
        if (!in_range || ue_lead_ms(frame, subframe, sfn, sf) != lead)
          report(frame, subframe, lead, sfn, sf, "the lead ahead, as the UE counts it");
      }
    }
  }

  // one ms more, and from the last subframe of a frame the epoch comes back to that frame: the UE would read it a cycle early
  {
    const int lead = NR_SIB19_MAX_EPOCH_LEAD_MS + 1;
    int sfn = -1, sf = -1;
    nr_sib19_epoch_from_lead(0, 9, lead, &sfn, &sf);
    if (ue_lead_ms(0, 9, sfn, sf) == lead)
      report(0, 9, lead, sfn, sf, "an epoch that the UE misreads, if the maximum lead is the longest one");
  }

  if (failures)
    printf("%d failures\n", failures);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
