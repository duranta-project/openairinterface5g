/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ran_func_dapp.h"

#include "openair2/E2AP/flexric/src/util/time_now_us.h"

#include <stdatomic.h>

static pthread_once_t once_dapp_mutex = PTHREAD_ONCE_INIT;
static dapp_subs_data_t dapp_subs_data = {0};

/* True once any RIC-originated callback (subscription/control) has reached
 * this RAN function; those only arrive over an established E2 connection, so
 * this is the closest "E2 setup completed" signal available outside the agent
 * (flexric exposes no connection state). Never cleared: a lost association
 * cannot be observed from here either. */
static atomic_bool e2_ric_seen = false;

/**
 * @brief Create a byte_array_t from a C-string.
 *
 * Allocates a new buffer, copies the content of @p s (without NUL terminator),
 * and returns a byte_array_t owning that buffer.
 */
static byte_array_t ba_from_cstr(const char* s)
{
  byte_array_t ba = {0};
  size_t len = strlen(s);
  ba.buf = malloc(len);
  assert(ba.buf != NULL && "Memory exhausted");
  memcpy(ba.buf, s, len);
  ba.len = len;
  return ba;
}

/**
 * @brief Fill the DAPP RAN function name/description/oid fields.
 *
 * Initializes the name, OID, and description of the given DAPP function
 * definition using constant strings; leaves instance as NULL.
 */
static void fill_dapp_ran_function_name(e2sm_dapp_func_def_t* def)
{
  assert(def != NULL);

  def->name.name = ba_from_cstr("E2SM-DAPP");

  def->name.oid = ba_from_cstr("1.3.6.1.4.1.53148.1.1.255.3");

  def->name.description = ba_from_cstr("E2SM-DAPP for xApp-dApp synchronization");

  def->name.instance = NULL;
}

/**
 * @brief Build the DAPP report style definitions.
 *
 * Allocates and initializes a ran_func_def_report_dapp_sm_t with three
 * report styles:
 *   - Style 1 (DAPP-E3-DATA-REPORT): E3 data reports only.
 *     ind_hdr_type=1, ind_msg_type=1.
 *   - Style 2 (DAPP-E3-SUBSCRIPTION-MAP): E3 subscription map only.
 *     ind_hdr_type=2, ind_msg_type=2.
 */
static ran_func_def_report_dapp_sm_t* make_dapp_report_def(void)
{
  ran_func_def_report_dapp_sm_t* rpt = calloc(1, sizeof(*rpt));
  assert(rpt != NULL && "Memory exhausted");

  rpt->sz_seq_report_sty = 2;
  rpt->seq_report_sty = calloc(rpt->sz_seq_report_sty, sizeof(seq_report_sty_dapp_sm_t));
  assert(rpt->seq_report_sty != NULL && "Memory exhausted");

  /* Style 1: E3 data reports only */
  rpt->seq_report_sty[0].report_type = DAPP_RIC_STYLE_E3_DATA_REPORT;
  rpt->seq_report_sty[0].name = ba_from_cstr("DAPP-E3-DATA-REPORT");
  rpt->seq_report_sty[0].ind_hdr_type = 1;
  rpt->seq_report_sty[0].ind_msg_type = 1;

  /* Style 2: E3 subscription map only */
  rpt->seq_report_sty[1].report_type = DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP;
  rpt->seq_report_sty[1].name = ba_from_cstr("DAPP-E3-SUBSCRIPTION-MAP");
  rpt->seq_report_sty[1].ind_hdr_type = 2;
  rpt->seq_report_sty[1].ind_msg_type = 2;

  return rpt;
}

/**
 * @brief Build the DAPP control style definition.
 *
 * Allocates and initializes a ran_func_def_ctrl_dapp_sm_t with a single
 * control style (style 1) for the DAPP RAN function definition.
 */
static ran_func_def_ctrl_dapp_sm_t* make_dapp_ctrl_def(void)
{
  ran_func_def_ctrl_dapp_sm_t* ctrl = calloc(1, sizeof(*ctrl));
  assert(ctrl != NULL && "Memory exhausted");

  ctrl->sz_seq_ctrl_style = 1;
  ctrl->seq_ctrl_style = calloc(ctrl->sz_seq_ctrl_style, sizeof(seq_ctrl_style_dapp_sm_t));
  assert(ctrl->seq_ctrl_style != NULL && "Memory exhausted");

  seq_ctrl_style_dapp_sm_t* s = &ctrl->seq_ctrl_style[0];

  s->style_type = DAPP_RIC_STYLE_E3_DATA_REPORT;
  s->name = ba_from_cstr("DAPP-CONTROL-STYLE-1");
  s->hdr = 1;
  s->msg = 1;
  s->out_frmt = 1;

  return ctrl;
}

static void init_once_dapp(void)
{
  init_dapp_subs_data(&dapp_subs_data);
}

/**
 * @brief Build a dapp_e3_subscription_list_t from the current E3 subscription map.
 *
 * Queries the E3 agent for the current dApp subscription map, filters
 * out dApps with zero subscriptions, and returns a heap-allocated
 * subscription list. Returns NULL if no valid subscriptions exist.
 *
 * Caller takes ownership of the returned pointer and must free it
 * via free_dapp_e3_subscription_list() + free().
 */
static dapp_e3_subscription_list_t* build_dapp_e3_subs_from_map(void)
{
  e3_dapp_subscription_map_t map = e3_get_dapp_subscription_map();

  size_t valid_count = 0;
  for (size_t i = 0; i < map.num_dapps; i++) {
    if (map.dapps[i].num_e3_ran_funcs > 0) {
      valid_count++;
    }
  }

  if (valid_count == 0) {
    e3_free_dapp_subscription_map(&map);
    return NULL;
  }

  dapp_e3_subscription_list_t* subs = calloc(1, sizeof(dapp_e3_subscription_list_t));
  assert(subs != NULL && "Memory exhausted");
  subs->sz_dapp_e3_subscriptions = valid_count;
  subs->dapp_e3_subscriptions = calloc(valid_count, sizeof(dapp_e3_subscription_item_t));
  assert(subs->dapp_e3_subscriptions != NULL && "Memory exhausted");

  size_t idx = 0;
  for (size_t i = 0; i < map.num_dapps; i++) {
    if (map.dapps[i].num_e3_ran_funcs == 0) {
      continue;
    }

    subs->dapp_e3_subscriptions[idx].dapp_id = map.dapps[i].dapp_id;
    subs->dapp_e3_subscriptions[idx].sz_subscribed_e3_ran_functions = map.dapps[i].num_e3_ran_funcs;

    size_t sz = map.dapps[i].num_e3_ran_funcs * sizeof(uint32_t);
    subs->dapp_e3_subscriptions[idx].subscribed_e3_ran_functions = malloc(sz);
    assert(subs->dapp_e3_subscriptions[idx].subscribed_e3_ran_functions != NULL && "Memory exhausted");
    memcpy(subs->dapp_e3_subscriptions[idx].subscribed_e3_ran_functions, map.dapps[i].e3_ran_func_ids, sz);
    idx++;
  }

  e3_free_dapp_subscription_map(&map);
  return subs;
}

/**
 * @brief Build the complete DAPP RAN function definition.
 *
 * Fills the RAN function name, event trigger, report styles, and control
 * styles. The current dApp E3 subscription map is attached to:
 *   - Report style 1 (E3 data reports)
 *   - Control style 1
 *
 * Report style 2 (subscription map management) does not carry subscriptions.
 */
static e2sm_dapp_func_def_t fill_dapp_ran_def(void)
{
  e2sm_dapp_func_def_t def = (e2sm_dapp_func_def_t){0};

  fill_dapp_ran_function_name(&def);

  def.ev_trig = NULL;

  def.report = make_dapp_report_def();

  def.ctrl = make_dapp_ctrl_def();

  dapp_e3_subscription_list_t* subs = build_dapp_e3_subs_from_map();

  if (subs != NULL) {
    assert(def.report != NULL && def.report->sz_seq_report_sty >= 1);
    def.report->seq_report_sty[0].dapp_e3_subs = subs;

    assert(def.ctrl != NULL && def.ctrl->sz_seq_ctrl_style >= 1);
    def.ctrl->seq_ctrl_style[0].dapp_e3_subs = calloc(1, sizeof(dapp_e3_subscription_list_t));
    assert(def.ctrl->seq_ctrl_style[0].dapp_e3_subs != NULL && "Memory exhausted");
    *def.ctrl->seq_ctrl_style[0].dapp_e3_subs = cp_dapp_e3_subscription_list(subs);
  }

  return def;
}

/**
 * @brief Populate the DAPP RAN function definition during E2 setup.
 *
 * This is called by the E2 agent when building the DAPP RAN function
 * setup structure. It:
 *  - Fills dapp->ran_func_def with the DAPP RAN function definition.
 *  - Ensures global DAPP subscription state is initialized once.
 */
void read_dapp_setup_sm(void* data)
{
  assert(data != NULL);

  dapp_e2_setup_t* dapp = (dapp_e2_setup_t*)data;

  dapp->ran_func_def = fill_dapp_ran_def();

  const int ret = pthread_once(&once_dapp_mutex, init_once_dapp);
  DevAssert(ret == 0);
}

/**
 * @brief Send the dApp E3 subscription map to all Format-2 subscribers.
 *
 * Queries the current dApp E3 subscription map, wraps it in an
 * E2SM-DAPP IndicationHeader (format 2) + IndicationMessage (format 2),
 * and pushes one indication per registered Format-2 RIC subscription.
 */
static void generate_e2_indication_dapp_e3_subscriptions(void)
{
  /* E2-connection gate: RIC request IDs exist only for subscriptions installed
   * over an established E2 connection, so an empty snapshot means there is
   * nothing to send and no connection to send it over (the agent API
   * additionally drops async events if the agent is down). */
  uint32_t* ric_ids = NULL;
  size_t count = ric_subs_frmt2_snapshot(&ric_ids);
  if (count == 0 || ric_ids == NULL) {
    return;
  }
  e3_dapp_subscription_map_t map = e3_get_dapp_subscription_map();

  size_t valid_count = 0;
  for (size_t i = 0; i < map.num_dapps; i++) {
    if (map.dapps[i].num_e3_ran_funcs > 0) {
      valid_count++;
    }
  }

  /* MAC/RRC only provide the node identity for the header: absent on nr-cuup
   * (which registers this same RAN function but hosts no gNB MAC/RRC), the
   * identity fields are simply omitted. */
  const bool have_node_identity = RC.nrmac && RC.nrmac[0] && RC.nrrrc && RC.nrrrc[0];
  const f1ap_setup_req_t* f1_req = have_node_identity ? RC.nrmac[0]->f1_config.setup_req : NULL;
  const f1ap_served_cell_info_t* cell = (f1_req && f1_req->num_cells_available > 0) ? &f1_req->cell[0].info : NULL;

  for (size_t s = 0; s < count; ++s) {
    dapp_ind_data_t* ind = calloc(1, sizeof(*ind));
    assert(ind != NULL);

    // Header format 2: node identity only
    ind->hdr.format = FORMAT_2_E2SM_DAPP_IND_HDR;
    e2sm_dapp_ind_hdr_frmt_2_t* hdr = &ind->hdr.frmt_2;
    hdr->timestamp_ns = time_now_ns();

    if (f1_req != NULL && cell != NULL) {
      hdr->node_type = (uint8_t)RC.nrrrc[0]->node_type;
      hdr->node_nb_id = (uint32_t)(cell->nr_cellid >> 14);

      hdr->node_plmn_id[0] = (uint8_t)(cell->plmn.mcc >> 8);
      hdr->node_plmn_id[1] = (uint8_t)(cell->plmn.mcc & 0xFF);
      hdr->node_plmn_id[2] = (uint8_t)(cell->plmn.mnc & 0xFF);

      hdr->node_cu_du_id_present = true;
      hdr->node_cu_du_id = f1_req->gNB_DU_id;
    }

    // Message format 2: subscription map
    ind->msg.format = FORMAT_2_E2SM_DAPP_IND_MSG;
    e2sm_dapp_ind_msg_frmt_2_t* msg = &ind->msg.frmt_2;

    msg->dapp_e3_subs.sz_dapp_e3_subscriptions = valid_count;
    msg->dapp_e3_subs.dapp_e3_subscriptions = valid_count > 0 ? calloc(valid_count, sizeof(dapp_e3_subscription_item_t)) : NULL;

    size_t idx = 0;
    for (size_t i = 0; i < map.num_dapps; i++) {
      if (map.dapps[i].num_e3_ran_funcs == 0) {
        continue;
      }

      dapp_e3_subscription_item_t* dst = &msg->dapp_e3_subs.dapp_e3_subscriptions[idx];
      dst->dapp_id = map.dapps[i].dapp_id;
      dst->sz_subscribed_e3_ran_functions = map.dapps[i].num_e3_ran_funcs;

      size_t sz = map.dapps[i].num_e3_ran_funcs * sizeof(uint32_t);
      dst->subscribed_e3_ran_functions = malloc(sz);
      assert(dst->subscribed_e3_ran_functions != NULL);
      memcpy(dst->subscribed_e3_ran_functions, map.dapps[i].e3_ran_func_ids, sz);

      idx++;
    }

    /* No E3 payload for format 2 */
    ind->e3.type = DAPP_E3_SM_NONE;

    async_event_agent_api(ric_ids[s], ind);
  }

  e3_free_dapp_subscription_map(&map);
  free(ric_ids);
}

/* Called from the E3 agent whenever a dApp connects/disconnects; may therefore
 * fire with no E2 connection up. Both callees gate on the E2 side: the service
 * update is skipped until the RIC has been heard from (see below), and the
 * indication fan-out sends only to RIC request IDs of installed subscriptions. */
void notify_dapp_status_changed(void)
{
  /* Skip the RIC Service Update until E2 setup has completed: the agent sends
   * it synchronously on THIS thread, and with the association still
   * establishing the blocking SCTP send can hold the libe3 RX thread (the E3
   * control plane) for minutes. Nothing is lost while setup is pending -- the
   * agent regenerates the RAN-function definition, including the current dApp
   * subscription map, on every E2 SETUP REQUEST retry. */
  const bool ric_seen = atomic_load(&e2_ric_seen);
  if (ric_seen) {
    trigger_ric_service_update_api(SM_DAPP_ID);
  }
  generate_e2_indication_dapp_e3_subscriptions();
}

/**
 * @brief Convert an E3 DAPP report into one or more E2 indications.
 *
 * For each RIC subscription currently registered:
 *  - Allocates a dapp_ind_data_t,
 *  - Fills the E2SM-DAPP IndicationHeader (RAN function ID, dApp ID),
 *  - Wraps the E3 report bytes in the IndicationMessage format 1 payload,
 *  - Enqueues the indication on the FlexRIC async path.
 *
 * If @p report_data is NULL or @p report_size == 0, it returns immediately.
 */
void generate_e2_indication_from_e3_dapp_report(uint32_t ran_function_id,
                                                uint32_t dapp_id,
                                                size_t report_size,
                                                const uint8_t* report_data)
{
  if (report_data == NULL || report_size == 0) {
    return;
  }

  /* E2-connection gate: RIC request IDs exist only for subscriptions installed
   * over an established E2 connection (see
   * generate_e2_indication_dapp_e3_subscriptions). */
  uint32_t* ric_ids = NULL;
  size_t count = ric_subs_frmt1_snapshot(&ric_ids);
  if (count == 0 || ric_ids == NULL) {
    return;
  }

  /* Node identity for the header; absent on nr-cuup (no gNB MAC/RRC), the
   * identity fields are simply omitted. */
  const bool have_node_identity = RC.nrmac && RC.nrmac[0] && RC.nrrrc && RC.nrrrc[0];
  const f1ap_setup_req_t* f1_req = have_node_identity ? RC.nrmac[0]->f1_config.setup_req : NULL;
  const f1ap_served_cell_info_t* cell = (f1_req && f1_req->num_cells_available > 0) ? &f1_req->cell[0].info : NULL;

  for (size_t i = 0; i < count; ++i) {
    uint32_t ric_req_id = ric_ids[i];

    dapp_ind_data_t* ind = calloc(1, sizeof(*ind));
    assert(ind);

    ind->hdr.format = FORMAT_1_E2SM_DAPP_IND_HDR;
    e2sm_dapp_ind_hdr_frmt_1_t* hdr = &ind->hdr.frmt_1;
    hdr->ran_function_id = ran_function_id;
    hdr->dapp_id = dapp_id;
    hdr->timestamp_ns = time_now_ns();

    if (f1_req != NULL && cell != NULL) {
      hdr->node_type = (uint8_t)RC.nrrrc[0]->node_type;
      hdr->node_nb_id = (uint32_t)(cell->nr_cellid >> 14);

      hdr->node_plmn_id[0] = (uint8_t)(cell->plmn.mcc >> 8);
      hdr->node_plmn_id[1] = (uint8_t)(cell->plmn.mcc & 0xFF);
      hdr->node_plmn_id[2] = (uint8_t)(cell->plmn.mnc & 0xFF);

      hdr->node_cu_du_id_present = true;
      hdr->node_cu_du_id = f1_req->gNB_DU_id;
    }

    ind->msg.format = FORMAT_1_E2SM_DAPP_IND_MSG;
    e2sm_dapp_ind_msg_frmt_1_t* msg = &ind->msg.frmt_1;
    msg->data_size = report_size;
    msg->data = malloc(report_size);
    assert(msg->data != NULL);
    memcpy(msg->data, report_data, report_size);

    // Push to FlexRIC async path
    async_event_agent_api(ric_req_id, ind);
  }

  free(ric_ids);
}

/**
 * @brief Callback invoked when an aperiodic subscription is released.
 *
 * Removes the given RIC request ID from the internal DAPP subscription
 * bookkeeping.
 */
static void free_aperiodic_subscription(uint32_t ric_req_id)
{
  remove_dapp_subs_data(&dapp_subs_data, ric_req_id);
}

/**
 * @brief Handle DAPP SM subscription requests at the agent.
 *
 * Called when the RIC installs a subscription for the DAPP SM. For
 *  - Stores the RIC request ID in DAPP subscription data,
 *  - Registers the RIC subscription,
 *  - Returns an aperiodic subscription outcome with a custom free callback.
 *  - Supported ActionDefinition formats (currently FORMAT_1 only), it:
 */
sm_ag_if_ans_t write_subs_dapp_sm(void const* src)
{
  assert(src != NULL);
  atomic_store(&e2_ric_seen, true);
  wr_dapp_sub_data_t* wr_dapp = (wr_dapp_sub_data_t*)src;

  sm_ag_if_ans_t ans = {0};
  const uint32_t ric_req_id = wr_dapp->ric_req_id;
  assert(wr_dapp->dapp.action_def != NULL && "Action definition required");

  const uint32_t style = wr_dapp->dapp.action_def->ric_style_type;

  switch (style) {
    case DAPP_RIC_STYLE_E3_DATA_REPORT:
      /* E3 data reports only */
      insert_frmt_1_ric_id(&dapp_subs_data, ric_req_id);
      ric_subs_frmt1_add(ric_req_id);
      break;

    case DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP:
      /* E3 subscription map only */
      insert_frmt_2_ric_id(&dapp_subs_data, ric_req_id);
      ric_subs_frmt2_add(ric_req_id);
      break;

    default:
      AssertError(false, return ans, "[RAN FUNC DAPP SUBS] Unsupported Action Definition format %d\n", style);
  }

  ans.type = SUBS_OUTCOME_SM_AG_IF_ANS_V0;
  ans.subs_out.type = APERIODIC_SUBSCRIPTION_FLRC;
  ans.subs_out.aper.free_aper_subs = free_aperiodic_subscription;

  return ans;
}

/**
 * @brief Handle DAPP SM control requests at the agent.
 *
 * For format-1 E2SM-DAPP ControlMessages, and when compiled with E3_AGENT:
 *  - Extracts RAN function ID, dApp ID, and control payload,
 *  - Forwards it to the E3 agent via libe3 C API wrapper.
 *
 * Always returns a control outcome of type DAPP_AGENT_IF_CTRL_ANS_V0.
 */
sm_ag_if_ans_t write_ctrl_dapp_sm(void const* data)
{
  assert(data != NULL);
  atomic_store(&e2_ric_seen, true);

  dapp_ctrl_req_data_t const* ctrl = (dapp_ctrl_req_data_t const*)data;
  sm_ag_if_ans_t ans = {0};
  switch (ctrl->msg.format) {
    case FORMAT_1_E2SM_DAPP_CTRL_MSG: {
#if defined(E3_AGENT)
      uint32_t ran_function_id = ctrl->hdr.frmt_1.ran_function_id;
      uint32_t dapp_id = ctrl->hdr.frmt_1.dapp_id;

      uint32_t data_size = ctrl->msg.frmt_1.data_size;
      const uint8_t* payload = ctrl->msg.frmt_1.data;

      if (e3_send_xapp_control(dapp_id, ran_function_id, payload, data_size) != 0) {
        printf("[RAN FUNC DAPP CTRL] Failed to forward xApp control via E3 agent\n");
      }
#endif

      break;
    }

    default:
      AssertError(false, return ans, "[RAN FUNC DAPP CTRL] Unsupported Control Message format %d", ctrl->msg.format);
  }

  ans.type = CTRL_OUTCOME_SM_AG_IF_ANS_V0;
  ans.ctrl_out.type = DAPP_AGENT_IF_CTRL_ANS_V0;
  ans.ctrl_out.dapp.format = FORMAT_1_E2SM_DAPP_CTRL_OUT;
  ans.ctrl_out.dapp.frmt_1.timestamp_ns = time_now_ns();
  return ans;
}

/**
 * @brief Read DAPP SM state from the agent (not implemented).
 *
 * No READ operation is defined for the DAPP SM; this returns false (and never
 * aborts) so a stray READ cannot kill the gNB.
 */
bool read_dapp_sm(void* data)
{
  assert(data != NULL);
  /* No READ operation is defined for the DAPP SM. Return false rather than
   * assert(0)/abort, so a stray READ can never kill the gNB (and isn't a
   * silent fake-success under NDEBUG). */
  return false;
}
