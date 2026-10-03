/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Unpacking of the CUDA LDPC encoder's bit-sliced output. Input slices start at multiples of E words, so they
// are generally not 32-byte aligned; every buffer is placed against a PROT_NONE guard page so that reads past
// the input or writes past the output fault.

#include "gtest/gtest.h"
#include <cstdint>
#include <random>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
#include "openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_cuda/nrLDPC_cuda_unpack_output.h"
}

namespace {

// n_words of storage ending exactly at a PROT_NONE page
class guarded {
 public:
  explicit guarded(size_t n_words)
  {
    const size_t page = sysconf(_SC_PAGESIZE);
    const size_t bytes = n_words * sizeof(uint32_t);
    len = (bytes + page - 1) / page * page + page;
    base = (uint8_t *)mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    EXPECT_NE(base, MAP_FAILED);
    EXPECT_EQ(mprotect(base + len - page, page, PROT_NONE), 0);
    p = (uint32_t *)(base + len - page - bytes);
    memset(p, 0, bytes);
  }
  ~guarded() { munmap(base, len); }
  uint32_t *p;

 private:
  uint8_t *base;
  size_t len;
};

struct unpack_case {
  uint32_t E, E2, nb_segments, E2_first_segment;
};

// segment s, bit i, from the layout documented in nrLDPC_cuda_unpack_output.h
std::vector<uint8_t> reference_bits(const unpack_case &c, const uint32_t *f, const uint32_t *f2)
{
  const uint32_t E2_first_segment32 = c.E2_first_segment >> 5;
  std::vector<uint8_t> bits;
  for (uint32_t s = 0; s < c.nb_segments; s++) {
    const bool first = s < c.E2_first_segment;
    const uint32_t len = first ? c.E : c.E2;
    for (uint32_t i = 0; i < len; i++) {
      const uint32_t word = first ? f[(s >> 5) * c.E + i] : f2[((s >> 5) - E2_first_segment32) * c.E2 + i];
      bits.push_back((word >> (s & 31)) & 1);
    }
  }
  return bits;
}

using unpack_fn = void (*)(const uint32_t *, uint32_t, const uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint8_t *);

void check(unpack_fn unpack, const unpack_case &c, std::mt19937 &rng)
{
  SCOPED_TRACE(::testing::Message() << "E " << c.E << " E2 " << c.E2 << " nb_segments " << c.nb_segments << " E2_first_segment "
                                    << c.E2_first_segment);
  const uint32_t E2_first_segment32 = c.E2_first_segment >> 5;
  // groups as allocated by ldpcnblocks(): f covers groups [0, E2_first_segment32], f2 groups [E2_first_segment32, last]
  const uint32_t n_groups_f = c.E2_first_segment ? ((c.E2_first_segment - 1) >> 5) + 1 : 0;
  const uint32_t n_groups_f2 = c.nb_segments > c.E2_first_segment ? ((c.nb_segments - 1) >> 5) + 1 - E2_first_segment32 : 0;
  guarded f(std::max<size_t>(1, size_t(n_groups_f) * c.E));
  guarded f2(std::max<size_t>(1, size_t(n_groups_f2) * c.E2));
  for (size_t i = 0; i < size_t(n_groups_f) * c.E; i++)
    f.p[i] = rng();
  for (size_t i = 0; i < size_t(n_groups_f2) * c.E2; i++)
    f2.p[i] = rng();

  const std::vector<uint8_t> bits = reference_bits(c, f.p, f2.p);
  const size_t n_words = (bits.size() + 31) / 32;
  guarded out(n_words);
  unpack(f.p, c.E, f2.p, c.E2, E2_first_segment32, c.E2_first_segment, c.nb_segments, (uint8_t *)out.p);

  for (size_t k = 0; k < bits.size(); k++)
    ASSERT_EQ((out.p[k >> 5] >> (k & 31)) & 1, bits[k]) << "bit " << k;
  if (bits.size() & 31) {
    ASSERT_EQ(out.p[n_words - 1] >> (bits.size() & 31), 0U) << "bits set past the end";
  }
}

void check_all(const unpack_case &c, std::mt19937 &rng)
{
  check(nr_ldpc_cuda_unpack_output_scalar, c, rng);
#ifdef __AVX2__
  check(nr_ldpc_cuda_unpack_output_avx2, c, rng);
#endif
}

} // namespace

// nr_ulsim.ldpc_cuda.50seg_r89 on x86: E2 = 9420 (16-byte offset slices) and more than 32 segments
TEST(ldpc_cuda_unpack_output, ulsim_50seg_r89)
{
  std::mt19937 rng(1);
  for (uint32_t E2_first_segment : {0u, 10u, 32u, 33u, 50u})
    check_all({9432, 9420, 50, E2_first_segment}, rng);
}

TEST(ldpc_cuda_unpack_output, all_alignments_and_tails)
{
  std::mt19937 rng(2);
  for (uint32_t E = 1; E <= 80; E++)
    for (uint32_t nb_segments : {1u, 31u, 32u, 33u, 64u, 65u})
      check_all({E, E + (E & 1 ? 1u : 0u), nb_segments, nb_segments / 2}, rng);
}

TEST(ldpc_cuda_unpack_output, random)
{
  std::mt19937 rng(3);
  for (int n = 0; n < 500; n++) {
    unpack_case c;
    c.nb_segments = 1 + rng() % 152;
    c.E = 1 + rng() % 12000;
    c.E2 = std::max(1, int(c.E) + ((rng() % 2) ? 0 : int(rng() % 17) - 8));
    c.E2_first_segment = rng() % (c.nb_segments + 1);
    check_all(c, rng);
    if (::testing::Test::HasFatalFailure())
      return;
  }
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
