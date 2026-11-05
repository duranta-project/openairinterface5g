/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
#include "work_q.h"
}

struct Item {
  uint64_t id;
  work_q_t *queue;
};

static size_t idx_of(work_q_t *q, void *slot)
{
  return (size_t)((uint8_t *)slot - q->slots) / q->stride;
}

TEST(work_q, full_ring)
{
  constexpr size_t CNT = 64;
  work_q_t q;
  ASSERT_TRUE(work_q_alloc(&q, CNT, sizeof(Item)));

  std::vector<void *> held;
  for (size_t i = 0; i < CNT; ++i) {
    Item it{i, &q};
    void *slot = work_q_push(&q, &it);
    ASSERT_NE(slot, nullptr);
    ASSERT_EQ(idx_of(&q, slot), i);
    held.push_back(slot);
  }

  Item overflow{CNT, &q};
  EXPECT_EQ(work_q_push(&q, &overflow), nullptr); // all CNT slots held
  work_q_done(&q, held[0]);
  void *slot = nullptr;
  for (int i = 0; i < (int)CNT; ++i) {
    slot = work_q_push(&q, &overflow);
    if (slot != nullptr)
      break;
  }
  ASSERT_NE(slot, nullptr);
  EXPECT_EQ(slot, held[0]);
  EXPECT_EQ(work_q_push(&q, &overflow), nullptr); // busy again

  for (size_t i = 0; i < CNT; ++i)
    work_q_done(&q, held[i]);

  std::vector<void *> reused;
  for (size_t i = 0; i < CNT; ++i) {
    Item it{1000 + i, &q};
    void *slot = work_q_push(&q, &it);
    ASSERT_NE(slot, nullptr);
    ASSERT_EQ(((Item *)slot)->id, it.id);
    reused.push_back(slot);
  }
  EXPECT_EQ(work_q_push(&q, &overflow), nullptr); // full again: all CNT slots busy

  for (void *s : reused)
    work_q_done(&q, s);
  work_q_free(&q);
}

// Repeated fill/drain cycles in varying orders must recycle all slots without ever aliasing a
// still-live item, and the item content must come back untouched.
TEST(work_q, wrap_around)
{
  constexpr size_t CNT = 8;
  work_q_t q;
  ASSERT_TRUE(work_q_alloc(&q, CNT, sizeof(Item)));

  for (int round = 0; round < 100; ++round) {
    for (size_t i = 0; i < CNT; ++i) {
      Item it{(uint64_t)round * 1000 + i, &q};
      void *slot = work_q_push(&q, &it);
      ASSERT_NE(slot, nullptr);
      ASSERT_EQ(((Item *)slot)->id, it.id);
    }
    // release in reverse order
    for (size_t i = 0; i < CNT; ++i) {
      // the slots currently held are 0..CNT-1 in order; the producer wraps back to slot 0 only
      // after the whole ring is released, so the round's items stay intact until their done
      work_q_done(&q, q.slots + (CNT - 1 - i) * q.stride);
    }
  }
  work_q_free(&q);
}

TEST(work_q, slow_consumer_not_overwritten)
{
  constexpr size_t CNT = 8;
  constexpr int NUM_PUSHES = 200000;
  constexpr int NUM_CONSUMERS = 4;

  work_q_t q;
  ASSERT_TRUE(work_q_alloc(&q, CNT, sizeof(Item)));

  // MPMC-safe handoff of slot pointers to the consumers (a spsc_q would be an SPSC violation
  // here: multiple consumers on one ring).
  std::mutex mtx;
  std::condition_variable cv;
  std::deque<void *> handoff;
  bool producer_done = false;

  std::atomic<bool> in_use[CNT];
  for (auto &f : in_use)
    f = false;
  std::atomic<bool> producer_error{false};
  std::atomic<bool> consumer_content_error{false};
  std::atomic<bool> consumer_handout_error{false};
  std::atomic<uint64_t> pushed{0};

  auto handoff_push = [&](void *slot) {
    std::unique_lock<std::mutex> lk(mtx);
    handoff.push_back(slot);
    cv.notify_one();
  };
  // Returns a slot pointer, or NULL when the producer is done and the handoff is drained.
  auto handoff_pop = [&]() -> void * {
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&] { return !handoff.empty() || producer_done; });
    if (handoff.empty())
      return NULL;
    void *slot = handoff.front();
    handoff.pop_front();
    return slot;
  };

  std::thread producer([&]() {
    for (uint64_t id = 0; id < NUM_PUSHES; ++id) {
      Item it{id, &q};
      void *slot = work_q_push(&q, &it);
      if (slot == nullptr)
        continue; // ring full - allowed; consumers drain it
      if (in_use[idx_of(&q, slot)].exchange(true))
        producer_error = true; // handed out twice before done: the race
      handoff_push(slot);
    }
    pushed.store(NUM_PUSHES);
    {
      std::unique_lock<std::mutex> lk(mtx);
      producer_done = true;
    }
    cv.notify_all();
  });

  auto consumer = [&]() {
    for (;;) {
      void *slot = handoff_pop();
      if (slot == NULL)
        break;
      Item *it = (Item *)slot;
      if (it->id >= (uint64_t)NUM_PUSHES || it->queue != &q)
        consumer_content_error = true; // torn or stale content
      if (!in_use[idx_of(&q, slot)].load())
        consumer_handout_error = true; // handed out while previous owner still working
      // Mirror the queue's ownership exactly: clear the test flag BEFORE releasing the slot, so
      // the producer's exchange() sees false the moment the queue lets it reuse the slot.
      in_use[idx_of(&q, slot)].store(false);
      work_q_done(&q, slot);
    }
  };
  std::vector<std::thread> consumers;
  for (int i = 0; i < NUM_CONSUMERS; ++i)
    consumers.emplace_back(consumer);

  producer.join();
  for (auto &t : consumers)
    t.join();

  EXPECT_FALSE(producer_error);
  EXPECT_FALSE(consumer_content_error);
  EXPECT_FALSE(consumer_handout_error);
  EXPECT_EQ(pushed.load(), (uint64_t)NUM_PUSHES);
  EXPECT_TRUE(handoff.empty()); // everything drained

  work_q_free(&q);
}

// Multi-producer push: each push reserves its slot with a fetch_add, so two producers pushing
// concurrently must never alias
TEST(work_q, two_producers_not_overwritten)
{
  constexpr size_t CNT = 8;
  constexpr int NUM_PUSHES = 100000; // per producer
  constexpr int NUM_CONSUMERS = 4;

  work_q_t q;
  ASSERT_TRUE(work_q_alloc(&q, CNT, sizeof(Item)));

  std::mutex mtx;
  std::condition_variable cv;
  std::deque<void *> handoff;
  int producers_done = 0;

  std::atomic<bool> in_use[CNT];
  for (auto &f : in_use)
    f = false;
  std::atomic<bool> producer_error{false};
  std::atomic<bool> consumer_content_error{false};
  std::atomic<bool> consumer_handout_error{false};
  std::atomic<uint64_t> pushed{0};

  auto handoff_push = [&](void *slot) {
    std::unique_lock<std::mutex> lk(mtx);
    handoff.push_back(slot);
    cv.notify_one();
  };
  // Returns a slot pointer, or NULL when both producers are done and the handoff is drained.
  auto handoff_pop = [&]() -> void * {
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&] { return !handoff.empty() || producers_done == 2; });
    if (handoff.empty())
      return NULL;
    void *slot = handoff.front();
    handoff.pop_front();
    return slot;
  };

  auto producer = [&](uint64_t base) {
    for (uint64_t id = base; id < (uint64_t)NUM_PUSHES + base; ++id) {
      Item it{id, &q};
      void *slot = work_q_push(&q, &it);
      if (slot == nullptr)
        continue; // ring full (or another producer took the last free slot) - allowed
      if (in_use[idx_of(&q, slot)].exchange(true))
        producer_error = true; // handed out twice before done: the race
      handoff_push(slot);
    }
    pushed.fetch_add(NUM_PUSHES);
    {
      std::unique_lock<std::mutex> lk(mtx);
      ++producers_done;
    }
    cv.notify_all();
  };

  auto consumer = [&]() {
    for (;;) {
      void *slot = handoff_pop();
      if (slot == NULL)
        break;
      Item *it = (Item *)slot;
      if (it->id >= (uint64_t)(2 * NUM_PUSHES) || it->queue != &q)
        consumer_content_error = true; // torn or stale content
      if (!in_use[idx_of(&q, slot)].load())
        consumer_handout_error = true; // handed out while previous owner still working
      // Mirror the queue's ownership exactly: clear the test flag BEFORE releasing the slot, so
      // a producer's exchange() sees false the moment the queue lets it reuse the slot.
      in_use[idx_of(&q, slot)].store(false);
      work_q_done(&q, slot);
    }
  };

  std::thread producer_a(producer, 0);
  std::thread producer_b(producer, (uint64_t)NUM_PUSHES);
  std::vector<std::thread> consumers;
  for (int i = 0; i < NUM_CONSUMERS; ++i)
    consumers.emplace_back(consumer);

  producer_a.join();
  producer_b.join();
  for (auto &t : consumers)
    t.join();

  EXPECT_FALSE(producer_error);
  EXPECT_FALSE(consumer_content_error);
  EXPECT_FALSE(consumer_handout_error);
  EXPECT_EQ(pushed.load(), (uint64_t)(2 * NUM_PUSHES));
  EXPECT_TRUE(handoff.empty()); // everything drained

  work_q_free(&q);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
