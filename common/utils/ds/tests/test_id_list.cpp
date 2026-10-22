/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <vector>
#include "gtest/gtest.h"
extern "C" {
#include "id_list.h"
}

static std::vector<int> to_vec(const id_list_t *l)
{
  std::vector<int> v;
  for (int cur = l->head; cur >= 0; cur = l->next[cur])
    v.push_back(cur);
  return v;
}

TEST(id_list, tail_and_front)
{
  id_list_t l;
  id_list_init(&l, 8);
  EXPECT_TRUE(to_vec(&l).empty());
  EXPECT_EQ(l.tail, -1);

  id_list_push_back(&l, 3);
  id_list_push_back(&l, 1);
  id_list_push_front(&l, 5);
  EXPECT_EQ(to_vec(&l), std::vector<int>({5, 3, 1}));
  EXPECT_EQ(l.tail, 1);

  id_list_pop_front(&l);
  EXPECT_EQ(to_vec(&l), std::vector<int>({3, 1}));
  id_list_pop_front(&l);
  id_list_pop_front(&l);
  EXPECT_TRUE(to_vec(&l).empty());
  EXPECT_EQ(l.tail, -1);

  id_list_free(&l);
}

TEST(id_list, add_remove)
{
  id_list_t l;
  id_list_init(&l, 8);
  id_list_add(&l, 0);
  id_list_add(&l, 4);
  id_list_add(&l, 2);
  EXPECT_EQ(l.tail, 2);

  id_list_remove(&l, 4); // middle
  EXPECT_EQ(to_vec(&l), std::vector<int>({0, 2}));
  id_list_remove(&l, 2); // tail
  EXPECT_EQ(to_vec(&l), std::vector<int>({0}));
  EXPECT_EQ(l.tail, 0);
  id_list_push_back(&l, 7); // tail must be valid after removal
  EXPECT_EQ(to_vec(&l), std::vector<int>({0, 7}));
  id_list_remove(&l, 0); // head
  EXPECT_EQ(to_vec(&l), std::vector<int>({7}));
  EXPECT_EQ(l.tail, 7);

  id_list_free(&l);
}

TEST(id_list, resize)
{
  id_list_t l;
  id_list_init(&l, 2);
  id_list_push_back(&l, 1);
  id_list_push_back(&l, 0);
  id_list_resize(&l, 6);
  EXPECT_EQ(l.len, 6);
  id_list_push_back(&l, 5);
  EXPECT_EQ(to_vec(&l), std::vector<int>({1, 0, 5}));
  id_list_remove(&l, 5);
  id_list_resize(&l, 2);
  EXPECT_EQ(l.len, 2);
  EXPECT_EQ(to_vec(&l), std::vector<int>({1, 0}));
  id_list_free(&l);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
