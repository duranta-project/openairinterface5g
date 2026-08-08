/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * \brief data structures for L1/L2 interface modules
 */

#ifndef __NR_IF_MODULE_H__
#define __NR_IF_MODULE_H__

#include "common/platform_types.h"
#include "fapi_nr_ue_interface.h"
#include "openair2/NR_PHY_INTERFACE/NR_IF_Module.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/sidelink_nr_ue_interface.h"

typedef enum sl_sidelink_slot_type {

  SIDELINK_SLOT_TYPE_NONE = 0,
  SIDELINK_SLOT_TYPE_RX,
  SIDELINK_SLOT_TYPE_TX,
  SIDELINK_SLOT_TYPE_BOTH

} sl_sidelink_slot_type_t;

typedef struct NR_UL_TIME_ALIGNMENT NR_UL_TIME_ALIGNMENT_t;

typedef struct {
    /// module id
  module_id_t module_id;
  /// gNB index
  uint32_t gNB_index;
  /// component carrier id
  int cc_id;
  /// hyper frame number
  int hfn;
  /// frame 
  frame_t frame;
  /// slot
  int slot;

    /// NR UE FAPI-like P7 message, direction: L1 to L2
    /// data reception indication structure
    fapi_nr_rx_indication_t *rx_ind;
    /// ssb_index, if ssb is not present in current TTI, value set to -1
    int ssb_index;
    /// dci reception indication structure
    fapi_nr_dci_indication_t *dci_ind;

    /// PHY specific data structure that can be passed on to L2 via nr_downlink_indication_t and
    /// back to L1 via the nr_scheduled_response_t 
    void *phy_data;
} nr_downlink_indication_t;


typedef struct {
    /// module id
    module_id_t module_id;
    /// gNB index
    uint32_t gNB_index;
    /// component carrier id
    int cc_id;
    /// frame tx
    frame_t frame;
    /// slot tx
    uint32_t slot;

    /// dci reception indication structure
    fapi_nr_dci_indication_t *dci_ind;

    void *phy_data;

    /// whether a PUSCH PDU is scheduled for this slot (set by the caller before the pusch-data/control steps)
    bool pusch_present;

} nr_uplink_indication_t;

typedef struct {
    /// module id
    module_id_t module_id;
    /// gNB index
    uint32_t gNB_index;
    /// component carrier id
    int cc_id;
    /// hyper frame number rx
    int hfn_rx;
    /// frame rx
    frame_t frame_rx;
    /// slot rx
    uint32_t slot_rx;
    /// hyper frame number tx
    int hfn_tx;
    /// frame tx
    frame_t frame_tx;
    /// slot tx
    uint32_t slot_tx;
    // slot type rx or tx
    sl_sidelink_slot_type_t slot_type;

    /// NR UE FAPI-like P7 message, direction: L1 to L2
    /// data reception indication structure
    sl_nr_rx_indication_t *rx_ind;

    //SCI received by PHY sent to MAC here
    sl_nr_sci_indication_t *sci_ind;

    /// PHY specific data structure that can be passed on to L2 via nr_sidelink_indication_t and
    /// back to L1 via the nr_scheduled_response_t
    void *phy_data;
} nr_sidelink_indication_t;


// Downlink subframe P7

struct PHY_VARS_NR_UE_s;
struct NR_UE_MAC_INST_s;
typedef struct {
    /// module id
    module_id_t module_id; 
    /// component carrier id
    int CC_id;
    /// NR UE FAPI-like P7 message, direction: L2 to L1
    /// downlink transmission configuration request structure
    fapi_nr_dl_config_request_t *dl_config;

    /// uplink transmission configuration request structure
    fapi_nr_ul_config_request_t *ul_config;

    // Sidelink RX configuration request
    sl_nr_rx_config_request_t *sl_rx_config;

    // Sidelink TX configuration request
    sl_nr_tx_config_request_t *sl_tx_config;

    /// PHY data structure initially passed on to L2 via the nr_downlink_indication_t and
    /// returned to L1 via nr_scheduled_response_t
    void *phy_data;
    struct NR_UE_MAC_INST_s *mac;
} nr_scheduled_response_t;

typedef struct {
    /// module id
    uint8_t Mod_id;
    /// component carrier id
    uint8_t CC_id;
    
    /// NR UE FAPI-like P5 message
    /// physical layer configuration request structure
    fapi_nr_config_request_t config_req;

} nr_phy_config_t;

typedef struct {
    /// module id
    uint8_t Mod_id;
    /// component carrier id
    uint8_t CC_id;

    /// NR UE FAPI-like P5 message
    /// physical layer configuration request structure
    sl_nr_phy_config_request_t sl_config_req;

} nr_sl_phy_config_t;

typedef struct {
    /// module id
    uint8_t Mod_id;
    /// component carrier id
    uint8_t CC_id;
    /// Flag signaling that synch_request was received
    uint8_t received_synch_request;
    /// NR UE FAPI message
    fapi_nr_synch_request_t synch_req;
} nr_synch_request_t;

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef int8_t (nr_ue_scheduled_response_f)(nr_scheduled_response_t *scheduled_response);

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef void (nr_sl_ue_scheduled_response_f)(nr_scheduled_response_t *sl_scheduled_response);


/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 */
typedef void (nr_ue_phy_config_request_f)(nr_phy_config_t *phy_config);

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 */
typedef void(nr_ue_sl_phy_config_request_f)(nr_sl_phy_config_t *sl_phy_config);

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 */
typedef void (nr_ue_synch_request_f)(nr_synch_request_t *synch_request);


/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef int (nr_ue_dl_indication_f)(nr_downlink_indication_t *dl_info);

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef int (nr_ue_ul_indication_f)(nr_uplink_indication_t *ul_info);

typedef void (nr_ue_slot_indication_f)(uint8_t mod_id, bool is_tx);

typedef void (nr_ue_meas_ind_f)(module_id_t module_id,
                                uint32_t gNB_index,
                                uint16_t Nid_cell,
                                bool csi_meas,
                                bool is_neighboring_cell,
                                int rsrp_dBm);

/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef void (nr_ue_sl_indication_f)(nr_sidelink_indication_t *sl_info);

//  TODO check this stuff can be reuse of need modification
typedef struct nr_ue_if_module_s {
  nr_ue_scheduled_response_f *scheduled_response;
  nr_ue_phy_config_request_f *phy_config_request;
  nr_ue_sl_phy_config_request_f *sl_phy_config_request;
  nr_ue_synch_request_f      *synch_request;
  nr_ue_dl_indication_f      *dl_indication;
  nr_ue_ul_indication_f      *ul_indication;
  nr_ue_ul_indication_f      *ul_indication_control;
  nr_ue_sl_indication_f      *sl_indication;
  nr_ue_slot_indication_f    *slot_indication;
  nr_ue_meas_ind_f           *meas_ind;
  uint32_t cc_mask;
  uint32_t current_frame;
  uint32_t current_slot;
  //pthread_mutex_t nr_if_mutex;
} nr_ue_if_module_t;


/**\brief reserved one of the interface(if) module instantce from pointer pool and done memory allocation by module_id.
   \param module_id module id*/
nr_ue_if_module_t *nr_ue_if_module_init(uint32_t module_id);

/**\brief interface between L1/L2, indicating the downlink related information, like dci_ind and rx_req
   \param dl_info including dci_ind and rx_request messages*/
int nr_ue_dl_indication(nr_downlink_indication_t *dl_info);

int nr_ue_ul_indication(nr_uplink_indication_t *ul_info);
int nr_ue_ul_indication_control(nr_uplink_indication_t *ul_info);

void nr_ue_sl_indication(nr_sidelink_indication_t *sl_indication);

void print_ue_mac_stats(const module_id_t mod, const int frame_rx, const int slot_rx);

#endif
