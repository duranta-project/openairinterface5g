/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __COMMON_UTILS_ASSERTIONS__H__
#define __COMMON_UTILS_ASSERTIONS__H__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include "common/platform_types.h"

#define OAI_EXIT_NORMAL 0
#define OAI_EXIT_ASSERT 1

#define _Assert_Exit_							\
  if (getenv("OAI_GDBSTACKS")) {						\
    char tmp [1000];							\
    sprintf(tmp,"gdb -ex='set confirm off' -ex 'thread apply all bt' -ex q -p %d < /dev/null", getpid());  \
    __attribute__((unused)) int dummy=system(tmp);						\
  }									\
  fprintf(stderr, "\nExiting execution\n");				\
  fflush(stdout);							\
  fflush(stderr);							\
  abort();

#define _Assert_(cOND, aCTION, fORMAT, aRGS...)             \
do {                                                        \
    if (!(cOND)) {                                          \
        fprintf(stderr, "\nAssertion (%s) failed!\n"   \
                "In %s() %s:%d\n" fORMAT,                   \
                #cOND, __FUNCTION__, __FILE__, __LINE__, ##aRGS);  \
        aCTION;                                             \
    }						\
} while(0)

#define AssertFatal(cOND, fORMAT, aRGS...)          _Assert_(cOND, _Assert_Exit_, fORMAT, ##aRGS)

#define AssertError(cOND, aCTION, fORMAT, aRGS...)  _Assert_(cOND, aCTION, fORMAT, ##aRGS)



#define DevCheck(cOND, vALUE1, vALUE2, vALUE3)                                                          \
_Assert_(cOND, _Assert_Exit_, #vALUE1 ": %" PRIdMAX "\n" #vALUE2 ": %" PRIdMAX "\n" #vALUE3 ": %" PRIdMAX "\n\n",  \
         (intmax_t)vALUE1, (intmax_t)vALUE2, (intmax_t)vALUE3)

#define DevCheck4(cOND, vALUE1, vALUE2, vALUE3, vALUE4)                                                                         \
_Assert_(cOND, _Assert_Exit_, #vALUE1": %" PRIdMAX "\n" #vALUE2 ": %" PRIdMAX "\n" #vALUE3 ": %" PRIdMAX "\n" #vALUE4 ": %" PRIdMAX "\n\n",   \
         (intmax_t)vALUE1, (intmax_t)vALUE2, (intmax_t)vALUE3, (intmax_t)vALUE4)

#define DevParam(vALUE1, vALUE2, vALUE3)    DevCheck(0, vALUE1, vALUE2, vALUE3)

#define DevAssert(cOND)                     _Assert_(cOND, _Assert_Exit_, "")

#define DevMessage(mESSAGE)                 _Assert_(0, _Assert_Exit_, #mESSAGE)

#endif /* __COMMON_UTILS_ASSERTIONS__H__ */
