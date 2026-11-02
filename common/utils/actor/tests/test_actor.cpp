/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <assert.h>
#include <threads.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include "common/config/config_userapi.h"
#include "log.h"

extern "C" {
#include "actor.h"
configmodule_interface_t *uniqCfg;
}

int num_processed = 0;

void process(void *arg)
{
  num_processed++;
}

TEST(actor, schedule_one)
{
  Actor_t actor;
  init_actor(&actor, "TEST", -1);
  notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, NULL, process);
  pushNotifiedFIFO(&actor.fifo, elt);
  shutdown_actor(&actor);
  EXPECT_EQ(num_processed, 1);
}

TEST(actor, schedule_many)
{
  num_processed = 0;
  Actor_t actor;
  init_actor(&actor, "TEST", -1);
  for (int i = 0; i < 100; i++) {
    notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, NULL, process);
    pushNotifiedFIFO(&actor.fifo, elt);
  }
  shutdown_actor(&actor);
  EXPECT_EQ(num_processed, 100);
}

// Thread safety can be ensured through core affinity - if two actors
// are running on the same core they are thread safe
TEST(DISABLED_actor, thread_safety_with_core_affinity)
{
  num_processed = 0;
  int core = 0;
  Actor_t actor;
  init_actor(&actor, "TEST", core);
  Actor_t actor2;
  init_actor(&actor2, "TEST2", core);
  for (int i = 0; i < 10000; i++) {
    Actor_t *actor_ptr = i % 2 == 0 ? &actor : &actor2;
    notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, NULL, process);
    pushNotifiedFIFO(&actor_ptr->fifo, elt);
  }
  shutdown_actor(&actor);
  shutdown_actor(&actor2);
  EXPECT_EQ(num_processed, 10000);
}

// Thread safety can be ensured through data separation, here using C inheritance-like
// model
typedef struct {
  Actor_t actor;
  int count;
} TestActor_t;

void process_thread_safe(void *arg)
{
  TestActor_t *actor = static_cast<TestActor_t *>(arg);
  actor->count += 1;
}

TEST(actor, thread_safety_with_actor_specific_data)
{
  num_processed = 0;
  int core = 0;
  TestActor_t actor;
  INIT_ACTOR(&actor, "TEST", core);
  actor.count = 0;
  TestActor_t actor2;
  INIT_ACTOR(&actor2, "TEST2", core);
  actor2.count = 0;
  for (int i = 0; i < 10000; i++) {
    TestActor_t *actor_ptr = i % 2 == 0 ? &actor : &actor2;
    notifiedFIFO_elt_t *elt = newNotifiedFIFO_elt(0, 0, NULL, process_thread_safe);
    elt->msgData = actor_ptr;
    pushNotifiedFIFO(&actor_ptr->actor.fifo, elt);
  }
  SHUTDOWN_ACTOR(&actor);
  SHUTDOWN_ACTOR(&actor2);
  EXPECT_EQ(actor.count + actor2.count, 10000);
}

int main(int argc, char **argv)
{
  logInit();
  g_log->log_component[UTIL].level = OAILOG_DEBUG;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
