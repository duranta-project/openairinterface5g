// Checks that the AVX2 conversions in ocsdr_conv.h give the same output as the scalar references, for 1 and 2
// hardware channels, 1 and 2 antennas, every tx_shift 0..15, lengths 0..70 and misaligned buffers, with random
// samples that include the saturation edges.
// Build: g++ -O2 -march=native -I.. test_ocsdr_conv.cpp -o test_ocsdr_conv
#include "ocsdr_conv.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static uint64_t z = 0x9e3779b97f4a7c15ull;
static uint64_t rnd()
{
  z += 0x9e3779b97f4a7c15ull;
  uint64_t x = z;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}
static int16_t sample()
{
  static const int16_t edge[] = {0, 1, -1, 8191, 8192, -8192, -8193, 16383, -16384, 32767, -32767, -32768};
  return (rnd() & 3) == 0 ? edge[rnd() % 12] : (int16_t)rnd();
}

int main()
{
#if !defined(__AVX2__)
  puts("built without AVX2: only the scalar path exists");
#endif
  long cases = 0, bad = 0;
  for (unsigned hw = 1; hw <= 2; hw++)
    for (unsigned nch = 1; nch <= hw; nch++)
      for (unsigned sh = 0; sh <= 15; sh++)
        for (unsigned n = 0; n <= 70; n++)
          for (unsigned mis = 0; mis < 4; mis++) {
            std::vector<int16_t> i0(2 * n + 8), i1(2 * n + 8), a(2 * hw * n + 8, 7), b(2 * hw * n + 8, 7);
            for (auto &v : i0) v = sample();
            for (auto &v : i1) v = sample();
            const int16_t *in[2] = {i0.data() + 2 * mis, i1.data() + 2 * mis};
            ocsdr_tx_pack_ref(a.data() + 2 * mis, in, nch, hw, n, sh);
            ocsdr_tx_pack(b.data() + 2 * mis, in, nch, hw, n, sh);
            cases++;
            if (a != b) {
              bad++;
              if (bad < 5) printf("TX mismatch hw %u nch %u sh %u n %u mis %u\n", hw, nch, sh, n, mis);
            }
            // RX: the same block words split back into nout antennas
            for (unsigned nout = 1; nout <= hw; nout++) {
              std::vector<uint32_t> w(hw * n + 8), o0(n + 8, 5), o1(n + 8, 5), p0(n + 8, 5), p1(n + 8, 5);
              for (auto &v : w) v = (uint32_t)rnd();
              uint32_t *ra[2] = {o0.data() + mis, o1.data() + mis}, *rb[2] = {p0.data() + mis, p1.data() + mis};
              ocsdr_rx_unpack_ref(ra, nout, w.data() + mis, hw, n);
              ocsdr_rx_unpack(rb, nout, w.data() + mis, hw, n);
              cases++;
              if (o0 != p0 || o1 != p1) {
                bad++;
                if (bad < 5) printf("RX mismatch hw %u nout %u n %u mis %u\n", hw, nout, n, mis);
              }
            }
          }
  // the scalar reference against the expression the driver used before (clamp of x << sh to +-INT16_MAX)
  for (int x = -32768; x <= 32767; x++)
    for (unsigned sh = 0; sh <= 15; sh++) {
      int32_t v = (int32_t)x * (1 << sh), want = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
      cases++;
      if (ocsdr_tx_sample((int16_t)x, sh) != want) bad++;
    }
  printf("%ld cases, %ld mismatches: %s\n", cases, bad, bad ? "FAIL" : "PASS");
  return bad != 0;
}
