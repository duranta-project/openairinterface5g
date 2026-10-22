/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <stdlib.h>
#include "id_list.h"
#include "common/utils/utils.h"

/*
 * Create a new id_list
 */
void id_list_init(id_list_t *list, int len)
{
  list->head = -1;
  list->next = malloc_or_fail(len * sizeof(*list->next));
  for (int i = 0; i < len; ++i)
    list->next[i] = -1;
  list->tail = -1;
  list->len = len;
}

/*
 * Resize an id_list
 */
void id_list_resize(id_list_t *list, int new_len)
{
  if (new_len == list->len)
    return;
  if (new_len > list->len) {
    /* list->head remains */
    const int old_len = list->len;
    list->next = realloc_or_fail(list->next, new_len * sizeof(*list->next));
    for (int i = old_len; i < new_len; ++i)
      list->next[i] = -1;
    /* list->tail remains */
    list->len = new_len;
  } else { /* new_len < len */
    assert(list->head < new_len);
    assert(list->tail < new_len);
    for (int i = 0; i < list->len; ++i)
      assert(list->next[i] < new_len);
    /* list->head remains */
    list->next = realloc_or_fail(list->next, new_len * sizeof(*list->next));
    /* list->tail remains */
    list->len = new_len;
  }
}

/*
 * Destroy an id_list
 */
void id_list_free(id_list_t *list)
{
  free(list->next);
}

/*
 * Add an ID to an id_list at the end, traversing the whole list. Note:
 * id_list_push_back() is a faster alternative, but this implementation ensures
 * we do not add an existing ID.
 */
void id_list_add(id_list_t *listP, int id)
{
  int *cur = &listP->head;
  while (*cur >= 0) {
    assert(*cur != id);
    cur = &listP->next[*cur];
  }
  *cur = id;
  if (listP->next[id] < 0)
    listP->tail = id;
}

/*
 * Remove an ID from an id_list
 */
void id_list_remove(id_list_t *listP, int id)
{
  int *cur = &listP->head;
  int *prev = &listP->head;
  while (*cur != -1 && *cur != id) {
    prev = cur;
    cur = &listP->next[*cur];
  }
  assert(*cur != -1);
  int *next = &listP->next[*cur];
  *cur = listP->next[*cur];
  *next = -1;
  listP->tail = *prev >= 0 && listP->next[*prev] >= 0 ? listP->tail : *prev;
}

/*
 * Add an ID to the tail of the id_list in O(1). Note that there is no
 * corresponding id_list_pop_back(), as we cannot set the tail backwards and
 * therefore need to go through the whole list (use id_list_remove())
 */
void id_list_push_back(id_list_t *listP, int id)
{
  int *last = listP->tail < 0 ? &listP->head : &listP->next[listP->tail];
  *last = id;
  listP->next[id] = -1;
  listP->tail = id;
}

/*
 * Add an ID to the front of the id_list in O(1)
 */
void id_list_push_front(id_list_t *listP, int id)
{
  const int ohead = listP->head;
  listP->head = id;
  listP->next[id] = ohead;
  if (listP->tail < 0)
    listP->tail = id;
}

/*
 * Remove an ID from the front of the id_list in O(1)
 */
void id_list_pop_front(id_list_t *listP)
{
  assert(listP->head >= 0);
  const int ohead = listP->head;
  listP->head = listP->next[ohead];
  listP->next[ohead] = -1;
  if (listP->head < 0)
    listP->tail = -1;
}
