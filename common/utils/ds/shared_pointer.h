/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef COMMON_UTILS_DS_SHARED_POINTER_H
#define COMMON_UTILS_DS_SHARED_POINTER_H

#include <pthread.h>

typedef struct {
  void *p;
  void (*free)(void *p);
  int count;
  pthread_mutex_t m;
} shared_pointer_t;

shared_pointer_t *new_shared_pointer(void *p, void (*free)(void *p));
void unref_shared_pointer(shared_pointer_t *p);
void ref_shared_pointer(shared_pointer_t *p);

#endif /* COMMON_UTILS_DS_SHARED_POINTER_H */
