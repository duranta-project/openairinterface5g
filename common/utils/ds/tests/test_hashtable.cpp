/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
extern "C" {
#include "hashtable.h"
}

TEST(hashtable, test_insert_remove) {
  hash_table_t* table = hashtable_create(250, NULL, NULL);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(table, 1, a), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_remove(table, 1), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_iterate_remove) {
  hash_table_t* table = hashtable_create(250, NULL, NULL);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(table, 1, a), HASH_TABLE_OK);
  auto iterator = hashtable_get_iterator(table);
  void* ptr;
  int num_items = 0;
  while(hashtable_iterator_getnext(&iterator, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 1);
  EXPECT_EQ(hashtable_remove(table, 1), HASH_TABLE_OK);
  num_items = 0;

  auto iterator2 = hashtable_get_iterator(table);
  while(hashtable_iterator_getnext(&iterator2, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 0);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_three_iterate_remove) {
  hash_table_t* table = hashtable_create(250, NULL, NULL);
  int* a1 = (int*)calloc(1, sizeof(*a1));
  int* a2 = (int*)calloc(1, sizeof(*a2));
  int* a3 = (int*)calloc(1, sizeof(*a3));
  EXPECT_EQ(hashtable_insert(table, 1, a1), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_insert(table, 2, a2), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_insert(table, 3, a3), HASH_TABLE_OK);
  auto iterator = hashtable_get_iterator(table);
  void* ptr;
  int num_items = 0;
  while(hashtable_iterator_getnext(&iterator, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 3);
  EXPECT_EQ(hashtable_remove(table, 1), HASH_TABLE_OK);
  num_items = 0;

  auto iterator2 = hashtable_get_iterator(table);
  while(hashtable_iterator_getnext(&iterator2, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 2);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_insert_empty_insert_interate) {
  hash_table_t* table = hashtable_create(250, NULL, NULL);
  int* a = (int*)calloc(1, sizeof(*a));
  EXPECT_EQ(hashtable_insert(table, 1, a), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_remove(table, 1), HASH_TABLE_OK);

  int* a2 = (int*)calloc(1, sizeof(*a2));
  EXPECT_EQ(hashtable_insert(table, 1, a2), HASH_TABLE_OK);
  auto iterator3 = hashtable_get_iterator(table);
  int num_items = 0;
  void* ptr;
  while(hashtable_iterator_getnext(&iterator3, &ptr)) {
    num_items++;
  }
  EXPECT_EQ(num_items, 1);

  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

/* keys with constant low bits (like the c_init of the DMRS gold sequences, see refsig.c) must spread over a
 * power of two table with the default hash function */
TEST(hashtable, test_default_hash_spreads_structured_keys)
{
  const hash_size_t size = 1024;
  hash_table_t* table = hashtable_create(size, NULL, hash_free_int_func);
  const int num_keys = 20 * 14;
  for (int k = 0; k < num_keys; k++) {
    const hash_key_t key = ((((hash_key_t)k + 1) * 3) << 17) | 2;
    EXPECT_EQ(hashtable_insert(table, key, (void*)1), HASH_TABLE_OK);
  }
  int longest = 0;
  for (hash_size_t n = 0; n < size; n++) {
    int len = 0;
    for (hash_node_t* node = table->nodes[n]; node; node = node->next)
      len++;
    longest = std::max(longest, len);
  }
  EXPECT_LE(longest, 8);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

TEST(hashtable, test_create_zero_size)
{
  EXPECT_EQ(hashtable_create(0, NULL, NULL), nullptr);
}

/* inserting the data already stored for a key must not free it */
TEST(hashtable, test_insert_same_data)
{
  hash_table_t* table = hashtable_create(250, NULL, NULL);
  int* a = (int*)calloc(1, sizeof(*a));
  *a = 42;
  EXPECT_EQ(hashtable_insert(table, 1, a), HASH_TABLE_OK);
  EXPECT_EQ(hashtable_insert(table, 1, a), HASH_TABLE_INSERT_OVERWRITTEN_DATA);
  void* ptr = NULL;
  EXPECT_EQ(hashtable_get(table, 1, &ptr), HASH_TABLE_OK);
  EXPECT_EQ(ptr, a);
  EXPECT_EQ(*(int*)ptr, 42);
  EXPECT_EQ(hashtable_destroy(&table), HASH_TABLE_OK);
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
