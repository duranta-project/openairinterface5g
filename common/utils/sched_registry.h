/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Small typed registry used by scheduler policies and observers.  Entries are
 * registered at load time, which lets policy objects remain independent from
 * the scheduler core.
 */

#ifndef SCHED_REGISTRY_H
#define SCHED_REGISTRY_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SCHED_REGISTRY_DECLARE(registry, fn_type)         \
  void registry##_register(const char *name, fn_type fn); \
  fn_type registry##_lookup(const char *name);            \
  const char *registry##_names(void)

#define SCHED_REGISTRY_DEFINE(registry, fn_type)                                               \
  typedef struct registry##_entry_s {                                                          \
    const char *name;                                                                          \
    fn_type fn;                                                                                \
    struct registry##_entry_s *next;                                                           \
  } registry##_entry_t;                                                                        \
  static registry##_entry_t *registry##_entries;                                               \
  void registry##_register(const char *name, fn_type fn)                                       \
  {                                                                                            \
    registry##_entry_t *entry = calloc(1, sizeof(*entry));                                     \
    if (entry == NULL)                                                                         \
      abort();                                                                                 \
    entry->name = name;                                                                        \
    entry->fn = fn;                                                                            \
    entry->next = registry##_entries;                                                          \
    registry##_entries = entry;                                                                \
  }                                                                                            \
  fn_type registry##_lookup(const char *name)                                                  \
  {                                                                                            \
    for (registry##_entry_t *entry = registry##_entries; entry != NULL; entry = entry->next)   \
      if (strcmp(entry->name, name) == 0)                                                      \
        return entry->fn;                                                                      \
    return NULL;                                                                               \
  }                                                                                            \
  const char *registry##_names(void)                                                           \
  {                                                                                            \
    static char names[512];                                                                    \
    names[0] = '\0';                                                                           \
    for (registry##_entry_t *entry = registry##_entries; entry != NULL; entry = entry->next) { \
      if (names[0] != '\0')                                                                    \
        strncat(names, ", ", sizeof(names) - strlen(names) - 1);                               \
      strncat(names, entry->name, sizeof(names) - strlen(names) - 1);                          \
    }                                                                                          \
    return names;                                                                              \
  }

#define SCHED_REGISTRY_ADD(registry, entry_name, fn)                         \
  static void __attribute__((constructor)) registry##_add_##entry_name(void) \
  {                                                                          \
    registry##_register(#entry_name, fn);                                    \
  }

#endif /* SCHED_REGISTRY_H */
