/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_PDCP_ENTITY_H_
#define _NR_PDCP_ENTITY_H_

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "nr_pdcp_sdu.h"
#include "openair3/SECU/secu_defs.h"

/* PDCP Formats according to clause 6.2 of 3GPP TS 38.323 */
/* SN Size applicable to SRBs, UM DRBs and AM DRBs */
#define SHORT_SN_SIZE 12
/* SN Size applicable to UM DRBs and AM DRBs */
#define LONG_SN_SIZE 18
/* Data PDU for SRBs and DRBs with 12 bits PDCP SN (unit: byte) */
#define SHORT_PDCP_HEADER_SIZE 2
/* Data PDU for DRBs with 18 bits PDCP SN (unit: byte) */
#define LONG_PDCP_HEADER_SIZE 3
/* MAC-I size (unit: byte) */
#define PDCP_INTEGRITY_SIZE 4
/* K keys have 128 bits length */
#define NR_K_KEY_SIZE 16

typedef enum {
  NR_PDCP_DRB_AM,
  NR_PDCP_DRB_UM,
  NR_PDCP_SRB
} nr_pdcp_entity_type_t;

typedef struct {
  int integrity_algorithm;
  int ciphering_algorithm;
  uint8_t integrity_key[NR_K_KEY_SIZE];
  uint8_t ciphering_key[NR_K_KEY_SIZE];
} nr_pdcp_entity_security_keys_and_algos_t;

typedef struct {
  uint32_t sn;
  uint32_t hfn;
} nr_pdcp_count_t;

typedef struct {
  //nr_pdcp_entity_type_t mode;
  /* PDU stats */
  /* TX */
  uint32_t txpdu_pkts;     /* aggregated number of tx packets */
  uint32_t txpdu_bytes;    /* aggregated bytes of tx packets */
  uint32_t txpdu_sn;       /* current sequence number of last tx packet (or TX_NEXT) */
  /* RX */
  uint32_t rxpdu_pkts;     /* aggregated number of rx packets */
  uint32_t rxpdu_bytes;    /* aggregated bytes of rx packets */
  uint32_t rxpdu_sn;       /* current sequence number of last rx packet (or  RX_NEXT) */
  /* TODO? */
  uint32_t rxpdu_oo_pkts;       /* aggregated number of out-of-order rx pkts  (or RX_REORD) */
  /* TODO? */
  uint32_t rxpdu_oo_bytes; /* aggregated amount of out-of-order rx bytes */
  uint32_t rxpdu_dd_pkts;  /* aggregated number of duplicated discarded packets (or dropped packets because of other reasons such as integrity failure) (or RX_DELIV) */
  uint32_t rxpdu_dd_bytes; /* aggregated amount of discarded packets' bytes */
  /* TODO? */
  uint32_t rxpdu_ro_count; /* this state variable indicates the COUNT value following the COUNT value associated with the PDCP Data PDU which triggered t-Reordering. (RX_REORD) */

  /* SDU stats */
  /* TX */
  uint32_t txsdu_pkts;     /* number of SDUs delivered */
  uint32_t txsdu_bytes;    /* number of bytes of SDUs delivered */

  /* RX */
  uint32_t rxsdu_pkts;     /* number of SDUs received */
  uint32_t rxsdu_bytes;    /* number of bytes of SDUs received */

  uint8_t  mode;               /* 0: PDCP AM, 1: PDCP UM, 2: PDCP TM */
} nr_pdcp_statistics_t;

typedef struct nr_pdcp_entity_s {
  nr_pdcp_entity_type_t type;

  /* functions provided by the PDCP module */
  void (*recv_pdu)(struct nr_pdcp_entity_s *entity, char *buffer, int size);
  int (*process_sdu)(struct nr_pdcp_entity_s *entity, char *buffer, int size, int sdu_id, char *pdu_buffer, int pdu_max_size);
  void (*delete_entity)(struct nr_pdcp_entity_s *entity);
  void (*release_entity)(struct nr_pdcp_entity_s *entity);
  void (*suspend_entity)(struct nr_pdcp_entity_s *entity);
  void (*reestablish_entity)(struct nr_pdcp_entity_s *entity, const nr_pdcp_entity_security_keys_and_algos_t *parameters);
  void (*get_stats)(struct nr_pdcp_entity_s *entity, nr_pdcp_statistics_t *out);

  /* set_security: pass -1 to parameters->integrity_algorithm / parameters->ciphering_algorithm
   *               to keep the corresponding current algorithm and key
   */
  void (*set_security)(struct nr_pdcp_entity_s *entity, const nr_pdcp_entity_security_keys_and_algos_t *parameters);

  /* check_integrity is used by RRC */
  bool (*check_integrity)(struct nr_pdcp_entity_s *entity,
                          const uint8_t *buffer,
                          int buffer_length,
                          const nr_pdcp_integrity_data_t *msg_integrity);

  void (*set_time)(struct nr_pdcp_entity_s *entity, uint64_t now);

  /* callbacks provided to the PDCP module */
  void (*deliver_sdu)(void *deliver_sdu_data,
                      struct nr_pdcp_entity_s *entity,
                      char *buf,
                      int size,
                      const nr_pdcp_integrity_data_t *msg_integrity);
  void *deliver_sdu_data;

  /* configuration variables */
  int rb_id;
  int pdusession_id;
  bool has_sdap_rx;
  bool has_sdap_tx;
  int sn_size;                  /* SN size, in bits */
  int t_reordering;             /* unit: ms, -1 for infinity */
  int discard_timer;            /* unit: ms, -1 for infinity */

  int sn_max;                   /* (2^SN_size) - 1 */
  int window_size;              /* 2^(SN_size - 1) */

  /* state variables */
  uint32_t tx_next;
  uint32_t rx_next;
  uint32_t rx_deliv;
  uint32_t rx_reord;

  /* set to the latest know time by the user of the module. Unit: ms */
  uint64_t t_current;

  /* timers (stores the ms of activation, 0 means not active) */
  uint64_t t_reordering_start;

  /* security */
  int has_ciphering;
  int has_integrity;
  nr_pdcp_entity_security_keys_and_algos_t security_keys_and_algos;
  stream_security_context_t *security_context;
  void (*cipher)(stream_security_context_t *security_context,
                 unsigned char *buffer, int length,
                 int bearer, uint32_t count, int direction);
  void (*decipher)(stream_security_context_t *security_context,
                   unsigned char *buffer, int length,
                   int bearer, uint32_t count, int direction);
  void (*free_security)(stream_security_context_t *security_context);
  stream_security_context_t *integrity_context;
  void (*integrity)(stream_security_context_t *integrity_context,
                 unsigned char *out,
                 unsigned char *buffer, int length,
                 int bearer, uint32_t count, int direction);
  void (*free_integrity)(stream_security_context_t *integrity_context);
  /* security/integrity algorithms need to know uplink/downlink information
   * which is reverse for gnb and ue, so we need to know if this
   * pdcp entity is for a gnb or an ue
   */
  int is_gnb;

  /* rx management */
  nr_pdcp_sdu_t *rx_list;
  int           rx_size;
  int           rx_maxsize;
  nr_pdcp_statistics_t stats;

  /* Keep tracks of whether the PDCP entity was suspended or not */
  bool entity_suspended;

  /* PDCP Count API */
  nr_pdcp_count_t (*get_pdcp_count_dl)(struct nr_pdcp_entity_s *entity);
  nr_pdcp_count_t (*get_pdcp_count_ul)(struct nr_pdcp_entity_s *entity);
  void (*set_pdcp_count_dl)(struct nr_pdcp_entity_s *entity, nr_pdcp_count_t count, int sn_size);
  void (*set_pdcp_count_ul)(struct nr_pdcp_entity_s *entity, nr_pdcp_count_t count, int sn_size);

} nr_pdcp_entity_t;

nr_pdcp_entity_t *new_nr_pdcp_entity(
    nr_pdcp_entity_type_t type,
    int is_gnb,
    int rb_id,
    int pdusession_id,
    bool has_sdap_rx,
    bool has_sdap_tx,
    void (*deliver_sdu)(void *deliver_sdu_data,
                        struct nr_pdcp_entity_s *entity,
                        char *buf,
                        int size,
                        const nr_pdcp_integrity_data_t *msg_integrity),
    void *deliver_sdu_data,
    int sn_size,
    int t_reordering,
    int discard_timer,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters);

/* Get maximum PDCP PDU size */
int nr_max_pdcp_pdu_size(sdu_size_t sdu_size);

#endif /* _NR_PDCP_ENTITY_H_ */
