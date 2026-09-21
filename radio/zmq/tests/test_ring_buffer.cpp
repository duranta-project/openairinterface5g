/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include "ring_buffer.h"

TEST(CircularBuffer, simplePushPop)
{
  ring_buffer cb(10);
  cf_t data[10];
  for (int i = 0; i < 10; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  ASSERT_EQ(cb.push_samples(data, 10), 0);

  cf_t read_data[10];
  ASSERT_EQ(cb.pop_samples(read_data, 10), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, data[i].r);
    ASSERT_EQ(read_data[i].i, data[i].i);
  }

  ASSERT_EQ(cb.size(), 0);
  ASSERT_EQ(cb.pop_samples(read_data, 10), 0);
}

TEST(CircularBuffer, overflow)
{
  ring_buffer cb(10);
  cf_t data[15];
  for (int i = 0; i < 15; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  ASSERT_EQ(cb.push_samples(data, 15), 5);
  cf_t read_data[10];
  ASSERT_EQ(cb.pop_samples(read_data, 10), 10);
  // The first 5 samples are lost due to overflow
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, i + 5);
    ASSERT_EQ(read_data[i].i, i + 6);
  }
}

TEST(CircularBuffer, partialPop)
{
  ring_buffer cb(10);
  cf_t data[10];
  for (int i = 0; i < 10; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  ASSERT_EQ(cb.push_samples(data, 10), 0);
  cf_t read_data[5];
  ASSERT_EQ(cb.pop_samples(read_data, 5), 5);
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
  ASSERT_EQ(cb.size(), 5);
  ASSERT_EQ(cb.pop_samples(read_data, 5), 5);
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(read_data[i].r, i + 5);
    ASSERT_EQ(read_data[i].i, i + 6);
  }
}

TEST(CircularBuffer, popMore)
{
  ring_buffer cb(10);
  cf_t data[10];
  for (int i = 0; i < 10; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  cb.push_samples(data, 10);
  cf_t read_data[15];
  ASSERT_EQ(cb.pop_samples(read_data, 15), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
}

TEST(CircularBuffer, newOverflow)
{
  ring_buffer cb(10);
  cf_t data1[8];
  for (int i = 0; i < 8; i++) {
    data1[i] = {(float)i, (float)(i + 1)};
  }
  ASSERT_EQ(cb.push_samples(data1, 8), 0);

  cf_t data2[4];
  for (int i = 0; i < 4; i++) {
    data2[i] = {(float)(i + 8), (float)(i + 9)};
  }
  ASSERT_EQ(cb.push_samples(data2, 4), 2);

  cf_t read_data[10];
  ASSERT_EQ(cb.pop_samples(read_data, 10), 10);
  // The first 2 samples are lost due to overflow
  for (int i = 0; i < 8; i++) {
    ASSERT_EQ(read_data[i].r, i + 2);
    ASSERT_EQ(read_data[i].i, i + 3);
  }
  ASSERT_EQ(read_data[8].r, 10);
  ASSERT_EQ(read_data[8].i, 11);
  ASSERT_EQ(read_data[9].r, 11);
  ASSERT_EQ(read_data[9].i, 12);
}

TEST(overflow_buffer, simplePushPop)
{
  overflow_buffer pb(10);
  cf_t data[10];
  for (int i = 0; i < 10; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  pb.push_samples(data, 10);
  ASSERT_EQ(pb.size(), 10);
  cf_t read_data[10];
  ASSERT_EQ(pb.pop_samples(read_data, 10), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
}

TEST(overflow_buffer, zeros_to_send)
{
  overflow_buffer pb(10);
  pb.push_zeros(10);
  cf_t data[5];
  for (int i = 0; i < 5; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  pb.push_samples(data, 5);
  ASSERT_EQ(pb.size(), 15);
  cf_t read_data[10];
  ASSERT_EQ(pb.pop_samples(read_data, 10), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, 0);
    ASSERT_EQ(read_data[i].i, 0);
  }
  ASSERT_EQ(pb.pop_samples(read_data, 10), 5);
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
}

TEST(overflow_buffer, no_zeros_to_send)
{
  overflow_buffer pb(10);
  cf_t data[10];
  for (int i = 0; i < 10; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  pb.push_samples(data, 10);
  ASSERT_EQ(pb.size(), 10);
  cf_t read_data[10];
  ASSERT_EQ(pb.pop_samples(read_data, 10), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
}

TEST(overflow_buffer, overflow)
{
  overflow_buffer pb(10);
  cf_t data[15];
  for (int i = 0; i < 15; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  pb.push_samples(data, 15);
  ASSERT_EQ(pb.size(), 15);
  cf_t read_data[15];
  ASSERT_EQ(pb.pop_samples(read_data, 15), 15);
  // The first 5 samples were lost due to overflow.
  int i = 0;
  for (; i < 5; i++) {
    ASSERT_EQ(read_data[i].r, 0);
    ASSERT_EQ(read_data[i].i, 0);
  }
  for (; i < 15; i++) {
    ASSERT_EQ(read_data[i].r, i);
    ASSERT_EQ(read_data[i].i, i + 1);
  }
  ASSERT_EQ(pb.size(), 0);
}

TEST(overflow_buffer, push_zeros)
{
  overflow_buffer pb(10);
  pb.push_zeros(5);
  ASSERT_EQ(pb.size(), 5);
  cf_t data[5];
  for (int i = 0; i < 5; i++) {
    data[i] = {(float)i, (float)(i + 1)};
  }
  pb.push_samples(data, 5);
  ASSERT_EQ(pb.size(), 10);
  cf_t read_data[10];
  ASSERT_EQ(pb.pop_samples(read_data, 10), 10);
  for (int i = 0; i < 10; i++) {
    if (i < 5) {
      ASSERT_EQ(read_data[i].r, 0);
      ASSERT_EQ(read_data[i].i, 0);
    } else {
      ASSERT_EQ(read_data[i].r, data[i - 5].r);
      ASSERT_EQ(read_data[i].i, data[i - 5].i);
    }
  }
  ASSERT_EQ(pb.size(), 0);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
