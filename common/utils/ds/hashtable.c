/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "hashtable.h"
#include "assertions.h"


/*
 * free int function
 * hash_free_int_func() is used when this hashtable is used to store int values as data (pointer = value).
 */

void hash_free_int_func(void *memoryP)
{
  UNUSED(memoryP);
}

//-------------------------------------------------------------------------------------------------------------------------------
/*
 * Default hash function
 * def_hashfunc() is the default used by hashtable_create() when the user didn't specify one.
 * The bucket is hash % size, so the hash must depend on all the key bits: OAI keys are often structured
 * (e.g. c_init = A << 17 + 2 * N_ID + n_SCID), and with an identity hash and a power of two size, all the
 * keys of a cell land in the same few buckets. Use the splitmix64 finalizer to mix the 64 key bits.
 */

static hash_size_t def_hashfunc(const uint64_t keyP)
{
  uint64_t x = keyP;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return (hash_size_t)x;
}

//-------------------------------------------------------------------------------------------------------------------------------
/*
 * Initialisation
 * hashtable_create() sets up the initial structure of the hash table. The user specified size will be allocated and initialized to
 * NULL. The user can also specify a hash function. If the hashfunc argument is NULL, a default hash function is used. If freefunc
 * is NULL, free() is used to release the data; pass hash_free_int_func to store values that must not be freed. If an error occurred
 * (including a size of 0), NULL is returned. All other values in the returned hash_table_t pointer should be released with
 * hashtable_destroy().
 */
hash_table_t *hashtable_create(const hash_size_t sizeP, hash_size_t (*hashfuncP)(const hash_key_t ), void (*freefuncP)(void *)) {
  hash_table_t *hashtbl = NULL;

  if (sizeP == 0) {
    return NULL;
  }

  if(!(hashtbl=malloc(sizeof(hash_table_t)))) {
    return NULL;
  }

  if(!(hashtbl->nodes=calloc(sizeP, sizeof(hash_node_t *)))) {
    free(hashtbl);
    return NULL;
  }

  hashtbl->size=sizeP;

  if(hashfuncP) hashtbl->hashfunc=hashfuncP;
  else hashtbl->hashfunc=def_hashfunc;

  if(freefuncP) hashtbl->freefunc=freefuncP;
  else hashtbl->freefunc=free;

  return hashtbl;
}
//-------------------------------------------------------------------------------------------------------------------------------
/*
 * Cleanup
 * The hashtable_destroy() walks through the linked lists for each possible hash value, and releases the elements. It also releases the nodes array and the hash_table_t.
 */
hashtable_rc_t hashtable_destroy(hash_table_t **hashtblP) {
  hash_size_t n;
  hash_node_t *node, *oldnode;

  if (*hashtblP == NULL) {
    return HASH_TABLE_BAD_PARAMETER_HASHTABLE;
  }

  for(n=0; n<(*hashtblP)->size; ++n) {
    node=(*hashtblP)->nodes[n];

    while(node) {
      oldnode=node;
      node=node->next;

      if (oldnode->data) {
        (*hashtblP)->freefunc(oldnode->data);
      }

      free(oldnode);
    }
  }

  free((*hashtblP)->nodes);
  free((*hashtblP));
  *hashtblP=NULL;
  return HASH_TABLE_OK;
}
//-------------------------------------------------------------------------------------------------------------------------------
hashtable_rc_t hashtable_is_key_exists (const hash_table_t *const hashtblP, const hash_key_t keyP)
//-------------------------------------------------------------------------------------------------------------------------------
{
  hash_node_t *node = NULL;
  hash_size_t  hash = 0;

  if (hashtblP == NULL) {
    return HASH_TABLE_BAD_PARAMETER_HASHTABLE;
  }

  hash=hashtblP->hashfunc(keyP)%hashtblP->size;
  node=hashtblP->nodes[hash];

  while(node) {
    if(node->key == keyP) {
      return HASH_TABLE_OK;
    }

    node=node->next;
  }

  return HASH_TABLE_KEY_NOT_EXISTS;
}

//-------------------------------------------------------------------------------------------------------------------------------
/*
 * Adding a new element
 * To make sure the hash value is not bigger than size, the result of the user provided hash function is used modulo size.
 * If the key exists, its data is replaced and the old data is released with freefunc, unless it is the same pointer.
 */
hashtable_rc_t hashtable_insert(hash_table_t *const hashtblP, const hash_key_t keyP, void *dataP) {
  hash_node_t *node = NULL;
  hash_size_t  hash = 0;

  if (hashtblP == NULL) {
    return HASH_TABLE_BAD_PARAMETER_HASHTABLE;
  }

  hash=hashtblP->hashfunc(keyP)%hashtblP->size;
  node=hashtblP->nodes[hash];

  while(node) {
    if(node->key == keyP) {
      if (node->data && node->data != dataP) {
        hashtblP->freefunc(node->data);
      }

      node->data=dataP;
      return HASH_TABLE_INSERT_OVERWRITTEN_DATA;
    }

    node=node->next;
  }

  if (!(node = malloc(sizeof(hash_node_t))))
    return HASH_TABLE_SYSTEM_ERROR;

  node->key=keyP;
  node->data=dataP;
  node->next = hashtblP->nodes[hash];
  hashtblP->nodes[hash]=node;
  return HASH_TABLE_OK;
}
//-------------------------------------------------------------------------------------------------------------------------------
/*
 * To remove an element from the hash table, we just search for it in the linked list for that hash value,
 * and remove it if it is found, releasing its data with freefunc. If it was not found, HASH_TABLE_KEY_NOT_EXISTS is returned.
 */
hashtable_rc_t hashtable_remove(hash_table_t *const hashtblP, const hash_key_t keyP) {
  hash_node_t *node, *prevnode=NULL;
  hash_size_t  hash = 0;

  if (hashtblP == NULL) {
    return HASH_TABLE_BAD_PARAMETER_HASHTABLE;
  }

  hash=hashtblP->hashfunc(keyP)%hashtblP->size;
  node=hashtblP->nodes[hash];

  while(node) {
    if(node->key == keyP) {
      if(prevnode) prevnode->next=node->next;
      else hashtblP->nodes[hash]=node->next;

      if (node->data) {
        hashtblP->freefunc(node->data);
      }

      free(node);
      return HASH_TABLE_OK;
    }

    prevnode=node;
    node=node->next;
  }

  return HASH_TABLE_KEY_NOT_EXISTS;
}
//-------------------------------------------------------------------------------------------------------------------------------
/*
 * Searching for an element is easy. We just search through the linked list for the corresponding hash value.
 * If we didn't find it, *dataP is set to NULL and HASH_TABLE_KEY_NOT_EXISTS is returned.
 */
hashtable_rc_t hashtable_get(const hash_table_t *const hashtblP, const hash_key_t keyP, void **dataP) {
  hash_node_t *node = NULL;
  hash_size_t  hash = 0;

  if (hashtblP == NULL) {
    *dataP = NULL;
    return HASH_TABLE_BAD_PARAMETER_HASHTABLE;
  }

  hash = hashtblP->hashfunc(keyP) % hashtblP->size;
  node=hashtblP->nodes[hash];

  while(node) {
    if(node->key == keyP) {
      *dataP = node->data;
      return HASH_TABLE_OK;
    }

    node=node->next;
  }

  *dataP = NULL;
  return HASH_TABLE_KEY_NOT_EXISTS;
}

/*
 * Iteration over all the data: the table must not be modified between hashtable_get_iterator() and the last
 * hashtable_iterator_getnext() call.
 */
hash_table_iterator_s hashtable_get_iterator(const hash_table_t *const hashtbl)
{
  return (hash_table_iterator_s){.index = 0, .node = hashtbl->nodes[0], .hashtbl = hashtbl};
}

bool hashtable_iterator_getnext(hash_table_iterator_s *iterator, void **dataP)
{
  // Iterate over table indexes
  while (iterator->node == NULL) {
    iterator->index++;
    if (iterator->index >= iterator->hashtbl->size) {
      *dataP = NULL;
      return false;
    }
    iterator->node = iterator->hashtbl->nodes[iterator->index];
  }
  *dataP = iterator->node->data;
  iterator->node = iterator->node->next;
  return true;
}
