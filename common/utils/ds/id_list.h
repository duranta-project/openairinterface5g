/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ID_LIST_H_
#define ID_LIST_H_

/*! \brief Singly linked list of integer IDs in [0, len), e.g., HARQ processes.
 * Links are stored in an array indexed by ID ("cursor" linked list), so an ID
 * can be in a list at most once. -1 marks the end/an empty list. */
typedef struct {
  int head;
  int *next;
  int tail;
  int len;
} id_list_t;

void id_list_init(id_list_t *listP, int len);
void id_list_resize(id_list_t *list, int new_len);
void id_list_free(id_list_t *list);
void id_list_add(id_list_t *listP, int id);
void id_list_remove(id_list_t *listP, int id);
void id_list_push_back(id_list_t *listP, int id);
void id_list_push_front(id_list_t *listP, int id);
void id_list_pop_front(id_list_t *listP);

#endif /* ID_LIST_H_ */
