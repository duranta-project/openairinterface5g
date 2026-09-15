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
  exit_function(__FILE__, __FUNCTION__, __LINE__, "_Assert_Exit_", OAI_EXIT_ASSERT); \
  abort(); // to avoid gcc warnings - never executed unless app-specific exit_function() does not exit() nor abort()

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

/* Same fatal behavior as AssertFatal, but for a code path that a legal,
 * spec-conformant input can reach and that simply isn't implemented yet --
 * as opposed to an internal invariant being violated. Still aborts (a gNB
 * that cannot correctly handle a legally-formed PDU must not silently
 * proceed and transmit something wrong), but the distinct macro name and
 * message prefix let crash triage (e.g. fuzzer findings) separate "known
 * feature gap" from "real invariant violation" without reading every call
 * site's message by hand. */
#define AssertFatalNotImplemented(cOND, fORMAT, aRGS...) _Assert_(cOND, _Assert_Exit_, "NOT IMPLEMENTED: " fORMAT, ##aRGS)

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

#define CHECK_INIT_RETURN(fCT)                                  \
do {                                                            \
    int fct_ret;                                                \
    if ((fct_ret = (fCT)) != 0) {                               \
        fprintf(stderr, "Function "#fCT" has failed\n"          \
        "returning %d\n", fct_ret);                             \
        fflush(stdout);                                         \
        fflush(stderr);                                         \
        exit(EXIT_FAILURE);                                     \
    }                                                           \
} while(0)

#endif /* __COMMON_UTILS_ASSERTIONS__H__ */
