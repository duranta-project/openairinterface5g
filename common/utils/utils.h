/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _UTILS_H
#define _UTILS_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <common/utils/assertions.h>

#define CHECK_MUTEX
#ifdef CHECK_MUTEX
#include <pthread.h>
#define pthread_mutex_init(MuT, mutexattr)                                 \
  ({                                                                       \
    int macroRet;                                                          \
    if (mutexattr)                                                         \
      macroRet = (pthread_mutex_init)((MuT), mutexattr);                   \
    else {                                                                 \
      pthread_mutexattr_t type;                                            \
      pthread_mutexattr_init(&type);                                       \
      pthread_mutexattr_settype(&type, PTHREAD_MUTEX_ERRORCHECK_NP);       \
      macroRet = (pthread_mutex_init)((MuT), &type);                       \
    }                                                                      \
    AssertFatal(!macroRet, "mutex init failed: %s\n", strerror(macroRet)); \
    macroRet;                                                              \
  })
#endif

#define sizeofArray(a) (sizeof(a)/sizeof(*(a)))
#define CHECK_INDEX(ARRAY, INDEX) assert((INDEX) < sizeofArray(ARRAY))

#define GET_ARRAY_MAX(arr, size, max_val)    \
  do {                                       \
    DevAssert((size) > 0);                   \
    (max_val) = (arr)[0];                    \
    for (size_t _i = 1; _i < (size); _i++) { \
      if ((arr)[_i] > (max_val)) {           \
        (max_val) = (arr)[_i];               \
      }                                      \
    }                                        \
  } while (0)

// Prevent double evaluation in max macro
#define cmax(a,b) ({ __typeof__ (a) _a = (a); \
                     __typeof__ (b) _b = (b); \
                     _a > _b ? _a : _b; })


#define cmax3(a,b,c) ( cmax(cmax(a,b), c) )  

// Prevent double evaluation in min macro
#define cmin(a,b) ({ __typeof__ (a) _a = (a); \
                     __typeof__ (b) _b = (b); \
                     _a < _b ? _a : _b; })

#define IS_BIT_SET(a, b) ((a >> b) & 1)
#define SET_BIT(a, b) (a | (1 << b))

#ifdef __cplusplus
#ifdef min
#undef min
#undef max
#endif
#else
#define max(a,b) cmax(a,b)
#define min(a,b) cmin(a,b)
#endif

#ifndef malloc16
#define malloc16(x) memalign(64, x + 64)
#endif
#define free16(y,x) free(y)
#define openair_free(y,x) free((y))
#define PAGE_SIZE 4096

#define free_and_zero(PtR) do {     \
    if (PtR) {           \
      free(PtR);         \
      PtR = NULL;        \
    }                    \
  } while (0)

static inline void *malloc16_clear( size_t size ) {
  void *ptr = memalign(64, size + 64);
  DevAssert(ptr);
  memset( ptr, 0, size );
  return ptr;
}

static inline void *calloc_or_fail(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);

  if (ptr == NULL) {
    fprintf(stderr, "Failed to calloc() %zu elements of %zu bytes: out of memory", nmemb, size);
    abort();
  }

  return ptr;
}

static inline void *malloc_or_fail(size_t size)
{
  void *ptr = malloc(size);

  if (ptr == NULL) {
    fprintf(stderr, "Failed to malloc() %zu bytes: out of memory", size);
    exit(EXIT_FAILURE);
  }

  return ptr;
}

#if !defined (msg)
# define msg(aRGS...) LOG_D(PHY, ##aRGS)
#endif
#ifndef malloc16
#define malloc16(x) memalign(64, x)
#endif

#define free16(y,x) free(y)
#define openair_free(y,x) free((y))
#define PAGE_SIZE 4096

#define virt_to_phys(x) (x)

// Converts an hexadecimal ASCII coded digit into its value. **
int hex_char_to_hex_value (char c);
// Converts an hexadecimal ASCII coded string into its value.**
int hex_string_to_hex_value (uint8_t *hex_value, const char *hex_string, int size);
// Converts a decimal ASCII coded string into its BCD encoded value.
int digit_string_to_bcd_value(uint8_t *bcd_value, const char *digit_string, int size);

/* Map task id to printable name. */
typedef struct {
  int id;
  char text[256];
} text_info_t;

#define TO_TEXT(LabEl, nUmID) {nUmID, #LabEl},
#define TO_ENUM(LabEl, nUmID) LabEl = nUmID,

char *itoa(int i);

#define STRINGIFY(S) #S
#define TO_STRING(S) STRINGIFY(S)
int read_version(const char *version, uint8_t *major, uint8_t *minor, uint8_t *patch);

#define findInList(keY, result, list, element_type) {\
    int i;\
    for (i=0; i<sizeof(list)/sizeof(element_type) ; i++)\
      if (list[i].key==keY) {\
        result=list[i].val;\
        break;\
      }\
    AssertFatal(i < sizeof(list)/sizeof(element_type), "List %s doesn't contain %s\n",#list, #keY); \
  }
#ifdef __cplusplus
}
#endif

#endif
