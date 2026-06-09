/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
extern "C" {
#include "hashtable.h"
}

TEST(hashtable, test_insert_remove) {
  hash_table_t table = hashtable_create(250, NULL, free);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(&table, 1, a), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_remove(&table, 1), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_iterate_remove) {
  hash_table_t table = hashtable_create(250, NULL, free);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(&table, 1, a), HASH_TABLE_OK);
  auto iterator = hashtable_get_iterator(&table);
  void* ptr;
  int num_items = 0;
  while(hashtable_iterator_getnext(&iterator, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 1);
  EXPECT_EQ(hashtable_remove(&table, 1), HASH_TABLE_OK);
  num_items = 0;

  auto iterator2 = hashtable_get_iterator(&table);
  while(hashtable_iterator_getnext(&iterator2, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 0);
  EXPECT_EQ(num_items, hashtable_num_entries(&table));
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_three_iterate_remove) {
  hash_table_t table = hashtable_create(250, NULL, free);
  int* a1 = (int*)calloc(1, sizeof(*a1));
  int* a2 = (int*)calloc(1, sizeof(*a2));
  int* a3 = (int*)calloc(1, sizeof(*a3));
  EXPECT_EQ(hashtable_insert(&table, 1, a1), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_insert(&table, 2, a2), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_insert(&table, 3, a3), HASH_TABLE_OK);
  auto iterator = hashtable_get_iterator(&table);
  void* ptr;
  int num_items = 0;
  while(hashtable_iterator_getnext(&iterator, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 3);
  EXPECT_EQ(num_items, hashtable_num_entries(&table));
  EXPECT_EQ(hashtable_remove(&table, 1), HASH_TABLE_OK);
  num_items = 0;

  auto iterator2 = hashtable_get_iterator(&table);
  while(hashtable_iterator_getnext(&iterator2, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 2);
  EXPECT_EQ(num_items, hashtable_num_entries(&table));
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_empty_insert_interate) {
  hash_table_t table = hashtable_create(250, NULL, free);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(&table, 1, a), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_remove(&table, 1), HASH_TABLE_OK);

  int* a2 = (int*)calloc(1, sizeof(*a2));
  EXPECT_EQ(hashtable_insert(&table, 1, a2), HASH_TABLE_OK);
  auto iterator3 = hashtable_get_iterator(&table);
  int num_items = 0;
  void* ptr;
  while(hashtable_iterator_getnext(&iterator3, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 1);
  EXPECT_EQ(num_items, hashtable_num_entries(&table));

  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
