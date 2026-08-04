/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief soft scope API interface implementation
 */
#include <stdio.h>
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"
#include "phy_scope_interface.h"

#define SOFTSCOPE_ENDFUNC_IDX 0

static  loader_shlibfunc_t scope_fdesc[]= {{"end_forms",NULL}};

int copyDataMutexInit(scopeData_t *p)
{
  return pthread_mutex_init(&p->copyDataMutex, NULL);
}

int load_softscope(char *exectype, void *initarg) {
  char libname[64];
  sprintf(libname,"%.10sscope",exectype);
  return load_module_shlib(libname,scope_fdesc,1,initarg);
}

int end_forms(void) {
  if (scope_fdesc[SOFTSCOPE_ENDFUNC_IDX].fptr) {
    scope_fdesc[SOFTSCOPE_ENDFUNC_IDX].fptr();
    return 0;
  }

  return -1;
}

void copyData(void *scopeData, enum scopeDataType type, void *dataIn, int elementSz, int colSz, int lineSz, int offset, metadata *meta)
{
  if (type >= EXTRA_SCOPE_TYPES) {
    return;
  }
  scopeData_t *tmp = (scopeData_t *)scopeData;

  if (tmp) {
    // Begin of critical zone between UE Rx threads that might copy new data at the same time:
    pthread_mutex_lock(&tmp->copyDataMutex);
    scopeGraphData_t *oldData = ((scopeGraphData_t **)tmp->liveData)[type];
    tmp->copyDataBufsIdx[type] = (tmp->copyDataBufsIdx[type] + 1) % COPIES_MEM;
    int newCopyDataIdx = tmp->copyDataBufsIdx[type];
    pthread_mutex_unlock(&tmp->copyDataMutex);
    // End of critical zone between UE Rx threads
    int oldDataSz = oldData ? oldData->dataSize : 0;
    int newSz = max(elementSz * colSz * (lineSz + offset), oldDataSz);
    // New data will be copied in a different buffer than the live one
    scopeGraphData_t *newData = tmp->copyDataBufs[type][newCopyDataIdx];
    if (newData == NULL || newData->dataSize < newSz) {
      scopeGraphData_t *ptr = (scopeGraphData_t *)realloc(newData, sizeof(scopeGraphData_t) + newSz);
      if (!ptr) {
        LOG_E(PHY, "can't realloc\n");
        return;
      } else {
        tmp->copyDataBufs[type][newCopyDataIdx] = ptr;
        if (!newData) // we have a new malloc
          *ptr = (scopeGraphData_t){0};
        newData = ptr;
        newData->dataSize = newSz;
      }
    }
    if (offset && oldDataSz) // we copy the previous buffer because we have as input only a part of
      memcpy(newData + 1, oldData + 1, oldDataSz);

    newData->elementSz = elementSz;
    newData->colSz = colSz;
    newData->lineSz = lineSz + offset;
    memcpy(((void *)(newData + 1)) + elementSz * colSz * offset, dataIn, elementSz * colSz * lineSz);

    // The new data just copied becomes live now
    ((scopeGraphData_t **)tmp->liveData)[type] = newData;
    // Signal to the scope thread that this type has fresh data (single writer per type;
    // aligned 64-bit load/store is atomic on the reader side).
    tmp->liveDataCnt[type]++;
  }
}
