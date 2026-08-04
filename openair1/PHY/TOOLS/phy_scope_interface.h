/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief softscope interface API include file
 */
#ifndef __PHY_SCOPE_INTERFACE_H__
#define __PHY_SCOPE_INTERFACE_H__

#ifdef __cplusplus
#include <atomic>
#ifndef _Atomic
#define _Atomic(X) std::atomic< X >
#endif
#endif

#include <openair1/PHY/defs_gNB.h>
#include <openair1/PHY/defs_nr_UE.h>

typedef struct {
  _Atomic(uint32_t) nb_total;
  _Atomic(uint32_t) nb_nack;
  _Atomic(uint32_t) blockSize;   // block size, to be used for throughput calculation
  _Atomic(uint16_t) nofRBs;
  _Atomic(uint8_t ) dl_mcs;
} extended_kpi_ue;

typedef struct {
  int *argc;
  char **argv;
  RU_t *ru;
  PHY_VARS_gNB *gNB;
} scopeParms_t;

enum scopeDataType {
  pbchDlChEstimateTime,
  pbchLlr,
  pbchRxdataF_comp,
  pdcchLlr,
  pdcchRxdataF_comp,
  pdschLlr,
  pdschRxdataF_comp,
  commonRxdataF,
  gNBRxdataF,
  psbchDlChEstimateTime,
  psbchLlr,
  psbchRxdataF_comp,
  gNBulDelay,
  MAX_SCOPE_TYPES,
  gNBPuschRxIq = MAX_SCOPE_TYPES,
  gNBPuschLlr,
  ueTimeDomainSamples,
  ueTimeDomainSamplesBeforeSync,
  gNbTimeDomainSamples,
  pdschChanEstimates,
  pdschRxdataF,
  // Standalone O-RU frequency-domain I/Q constellation taps. Each buffer holds one OFDM
  // symbol for ALL antenna ports (colSz = number of ports, antenna-major layout), so the
  // O-RU I/Q window shows a fixed 4 plots regardless of antenna count. The four categories
  // form the DL/UL x pre/post-channel matrix:
  //   oruDLpreFreq  : DL before the channel  (from 7.2 FH; clean L1 TX)
  //   oruDLpostFreq : DL after the channel   (vrtsim server-TX output; staged)
  //   oruULpreFreq  : UL before the channel  (generated on the UE; staged)
  //   oruULpostFreq : UL after the channel   (O-RU RX after FFT+rotation)
  oruDLpreFreq,
  oruDLpostFreq,
  oruULpreFreq,
  oruULpostFreq,
  EXTRA_SCOPE_TYPES
};

enum PlotTypeGnbIf {
  puschLLRe,
  puschIQe,
};

#define COPIES_MEM 4

typedef struct {
  int dataSize;
  int elementSz;
  int colSz;
  int lineSz;
} scopeGraphData_t;

typedef struct {
  int slot;
  int frame;
} metadata;

typedef struct scopeData_s {
  int *argc;
  char **argv;
  RU_t *ru;
  PHY_VARS_gNB *gNB;
  scopeGraphData_t *liveData[EXTRA_SCOPE_TYPES];
  void (*copyData)(void *, enum scopeDataType, void *data, int elementSz, int colSz, int lineSz, int offset, metadata *meta);
  pthread_mutex_t copyDataMutex;
  scopeGraphData_t *copyDataBufs[EXTRA_SCOPE_TYPES][COPIES_MEM];
  int copyDataBufsIdx[EXTRA_SCOPE_TYPES];
  // Bumped by copyData() on every new copy; scope-side plots use it to redraw only when the
  // RT path actually produced new samples (avoids repainting stale/empty buffers).
  uint64_t liveDataCnt[EXTRA_SCOPE_TYPES];
  void (*scopeUpdater)(enum PlotTypeGnbIf plotType, int numElements);
  bool (*tryLockScopeData)(enum scopeDataType type, int elementSz, int colSz, int lineSz, metadata *meta);
  void (*copyDataUnsafeWithOffset)(enum scopeDataType type, void *dataIn, size_t size, size_t offset, int copy_index);
  void (*unlockScopeData)(enum scopeDataType type);
  void (*dumpScopeData)(int slot, int frame, const char *cause_string);
} scopeData_t;

int load_softscope(char *exectype, void *initarg);
int end_forms(void) ;
int copyDataMutexInit(scopeData_t *);
void copyData(void *, enum scopeDataType type, void *dataIn, int elementSz, int colSz, int lineSz, int offset, metadata *meta);

#define UEscopeCopyWithMetadata(ue, type, ...) \
  if (ue->scopeData) {               \
    ((scopeData_t *)ue->scopeData)->copyData((scopeData_t *)ue->scopeData, type, ##__VA_ARGS__); \
  }
#define UEscopeCopy(ue, type, ...) \
  if (ue->scopeData) {               \
    metadata mt = {.slot = -1, .frame = -1}; \
    ((scopeData_t *)ue->scopeData)->copyData((scopeData_t *)ue->scopeData, type, ##__VA_ARGS__, &mt); \
  }
#define gNBscopeCopyWithMetadata(gnb, type, ...) \
  if (gnb->scopeData) {              \
    ((scopeData_t *)gnb->scopeData)->copyData((scopeData_t *)gnb->scopeData, type, ##__VA_ARGS__); \
  }
#define gNBscopeCopy(gnb, type, ...) \
  if (gnb->scopeData) {              \
    metadata mt = {.slot = -1, .frame = -1}; \
    ((scopeData_t *)gnb->scopeData)->copyData((scopeData_t *)gnb->scopeData, type, ##__VA_ARGS__, &mt); \
  }
#define GnbScopeUpdate(gnb, type, numElt) \
  if (gnb->scopeData)                     \
    ((scopeData_t *)gnb->scopeData)->scopeUpdater(type, numElt);

#define gNBTryLockScopeData(gnb, type, ...)                                          \
  gnb->scopeData && ((scopeData_t *)gnb->scopeData)->tryLockScopeData \
      && ((scopeData_t *)gnb->scopeData)->tryLockScopeData(type, ##__VA_ARGS__)

#define gNBscopeCopyUnsafe(gnb, type, ...) \
  scopeData_t *scope_data = (scopeData_t *)gnb->scopeData; \
  if (scope_data && scope_data->copyDataUnsafeWithOffset) { \
    scope_data->copyDataUnsafeWithOffset(type, ##__VA_ARGS__); \
  }

#define gNBunlockScopeData(gnb, type) \
  scopeData_t *scope_data = (scopeData_t *)gnb->scopeData; \
  if (scope_data && scope_data->unlockScopeData) { \
    scope_data->unlockScopeData(type); \
  }

#define gNBdumpScopeData(gnb, slot, frame, cause_string)          \
  do {                                                            \
    scopeData_t *scope_data = (scopeData_t *)gnb->scopeData;      \
    if (scope_data && scope_data->dumpScopeData) {                \
      scope_data->dumpScopeData((slot), (frame), (cause_string)); \
    }                                                             \
  } while (0)

#define UEScopeHasTryLock(ue) \
  (ue->scopeData && ((scopeData_t *)ue->scopeData)->tryLockScopeData)

#define UETryLockScopeData(ue, type, ...)                           \
  ue->scopeData && ((scopeData_t *)ue->scopeData)->tryLockScopeData \
      && ((scopeData_t *)ue->scopeData)->tryLockScopeData(type, ##__VA_ARGS__)

#define UEscopeCopyUnsafe(ue, type, ...) \
  scopeData_t *scope_data = (scopeData_t *)ue->scopeData; \
  if (scope_data && scope_data->copyDataUnsafeWithOffset) { \
    scope_data->copyDataUnsafeWithOffset(type, ##__VA_ARGS__); \
  }

#define UEunlockScopeData(ue, type) \
  scopeData_t *scope_data = (scopeData_t *)ue->scopeData; \
  if (scope_data && scope_data->unlockScopeData) { \
    scope_data->unlockScopeData(type); \
  }

#define UEdumpScopeData(ue, slot, frame, cause_string)            \
  do {                                                            \
    scopeData_t *scope_data = (scopeData_t *)ue->scopeData;       \
    if (scope_data && scope_data->dumpScopeData) {                \
      scope_data->dumpScopeData((slot), (frame), (cause_string)); \
    }                                                             \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif
extended_kpi_ue* getKPIUE();
#ifdef __cplusplus
}
#endif

#endif
