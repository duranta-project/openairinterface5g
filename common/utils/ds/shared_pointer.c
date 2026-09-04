/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "shared_pointer.h"
#include "common/utils/utils.h"

#define fatal(x, ...) do { \
    if (!(x)) { \
      fprintf(stderr, __VA_ARGS__); \
      fflush(stderr); \
      exit(1); \
    } \
  } while (0)

static void lock(shared_pointer_t *p)
{
  fatal(pthread_mutex_lock(&p->m) == 0, "pthread_mutex_lock failed\n");
}

static void unlock(shared_pointer_t *p)
{
  fatal(pthread_mutex_unlock(&p->m) == 0, "pthread_mutex_unlock failed\n");
}

shared_pointer_t *new_shared_pointer(void *p, void (*free)(void *p))
{
  shared_pointer_t *s = malloc_or_fail(sizeof(*s));
  s->p = p;
  s->free = free;
  s->count = 1;
  pthread_mutex_init(&s->m, NULL);
  return s;
}

void unref_shared_pointer(shared_pointer_t *p)
{
  lock(p);
  fatal(p->count, "unref_shared_pointer() called for a pointer with count == 0\n");
  p->count--;
  if (p->count) {
    unlock(p);
    return;
  }
  unlock(p);
  p->free(p->p);
  free(p);
}

void ref_shared_pointer(shared_pointer_t *p)
{
  lock(p);
  fatal(p->count, "ref_shared_pointer() called for a pointer with count == 0\n");
  p->count++;
  unlock(p);
}
