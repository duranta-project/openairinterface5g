/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common/ran_context.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "openair2/GNB_APP/gnb_config_ng.h"
#include "openair2/GNB_APP/gnb_paramdef.h"
#include "openair2/XNAP/xnap_gNB.h"
#include "openair3/SCTP/sctp_default_values.h"

RAN_CONTEXT_t RC;
THREAD_STRUCT thread_struct;
uint64_t downlink_frequency[MAX_NUM_CCs][4];
int64_t uplink_frequency_offset[MAX_NUM_CCs][4];
int oai_exit = 0;

/* Multi-instance support. */
static int nb_gnb_inst = 0;
static uint32_t *gnb_id_tab = NULL;
static uint32_t *tac_tab = NULL;
static uint16_t *mcc_tab = NULL;
static uint16_t *mnc_tab = NULL;
static uint8_t *mnc_len_tab = NULL;

static xnap_net_config_t *xn_net_config_tab = NULL;

int gnb_id = 0;
uint32_t tac = 1;
uint16_t mcc = 1;
uint16_t mnc = 1;
uint8_t mnc_len = 2;

char *gnb_ipv4_address_for_NGU = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  UNUSED(assert);
  LOG_E(GNB_APP, "error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

nfapi_mode_t nfapi_mod = -1;
void nfapi_setmode(nfapi_mode_t nfapi_mode)
{
  nfapi_mod = nfapi_mode;
}
nfapi_mode_t nfapi_getmode(void)
{
  return nfapi_mod;
}

ngran_node_t get_node_type()
{
  return ngran_gNB_CUUP;
}

configmodule_interface_t *uniqCfg = NULL;

/* Reads the current config Xn/SCTP parameters */
static xnap_net_config_t read_ip_config_xn_from_loaded_config(uint32_t list_idx)
{
  xnap_net_config_t nc = {0};
  paramdef_t XnCandidateParams[] = XN_CANDIDATE_PARAMS_DESC;
  paramlist_def_t XnCandidateList = {GNB_CONFIG_STRING_CANDIDATE_GNB_IPV4_ADDRESS_FOR_XNC, NULL, 0};
  paramdef_t SCTPParams[] = GNBSCTPPARAMS_DESC;
  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(aprefix, "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, list_idx, GNB_CONFIG_STRING_XN_PARAMETERS);
  config_getlist(config_get_if(), &XnCandidateList, XnCandidateParams, sizeofArray(XnCandidateParams), aprefix);
  AssertFatal(XnCandidateList.numelt <= XNAP_MAX_NB_CANDIDATES,
              "Xn candidates limit exceeded (%d > %d)\n", XnCandidateList.numelt, XNAP_MAX_NB_CANDIDATES);

  LOG_I(XNAP, "Number of candidate gNBs configured: %d\n", XnCandidateList.numelt);
  for (int l = 0; l < XnCandidateList.numelt; l++) {
    nc.nb_of_candidate_gNBs++;
    nc.candidate_gnb_address_for_xnc[l] =
        strdup(*gpd(XnCandidateList.paramarray[l], sizeofArray(XnCandidateParams), GNB_CONFIG_STRING_CANDIDATE_GNB_ADDRESS_FOR_XNC)->strptr);
    LOG_I(XNAP, "Candidate gNB %d address: %s \n", l + 1, nc.candidate_gnb_address_for_xnc[l]);
  }

  paramdef_t XnParams[] = XNPARAMS_DESC;
  const int nb_xn_params = sizeofArray(XnParams);
  config_get(config_get_if(), XnParams, nb_xn_params, aprefix);
  nc.gnb_port_for_xnc = (uint32_t)*gpd(XnParams, nb_xn_params, GNB_CONFIG_STRING_GNB_PORT_FOR_XNC)->uptr;
  char **gnb_xn_ip_strptr = gpd(XnParams, nb_xn_params, GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC)->strptr;
  AssertFatal((gnb_xn_ip_strptr != NULL) && (nc.gnb_port_for_xnc != 0),
              "gNB IP/Port not added in the CU/gNB configuration file\n");
  nc.gnb_xn_interface_ip_address = strdup(*gnb_xn_ip_strptr);

  nc.sctp_streams.sctp_out_streams =  SCTP_OUT_STREAMS;
  nc.sctp_streams.sctp_in_streams = SCTP_IN_STREAMS;
  sprintf(aprefix, "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, 0, GNB_CONFIG_STRING_SCTP_CONFIG);
  config_get(config_get_if(), SCTPParams, sizeofArray(SCTPParams), aprefix);
  nc.sctp_streams.sctp_out_streams =  (uint16_t) *(SCTPParams[GNB_SCTP_OUTSTREAMS_IDX].uptr);
  nc.sctp_streams.sctp_in_streams = (uint16_t) *(SCTPParams[GNB_SCTP_INSTREAMS_IDX].uptr);

  return nc;
}

xnap_net_config_t read_ip_config_xn(uint32_t gnb_idx)
{
  AssertFatal(xn_net_config_tab != NULL && (int)gnb_idx < nb_gnb_inst,
              "No cached Xn network config for gNB instance %u (nb_gnb_inst=%d)\n",
              gnb_idx, nb_gnb_inst);
  return xn_net_config_tab[gnb_idx];
}

xnap_setup_req_t read_ng_setup_info(const ngap_register_gnb_cnf_t *cnf, uint32_t gnb_idx)
{
  LOG_I(XNAP, "[gNB %u] Reading info required for Xn setup from NGAP_REGISTER_GNB_CNF\n", gnb_idx);
  xnap_setup_req_t setup_info = {0};

  setup_info.gNB_id = cnf->gNB_id;
  if (cnf->num_plmn > 0)
    setup_info.plmn = cnf->plmn[0].plmn;

  setup_info.num_tai = 1;
  setup_info.tai_support = calloc_or_fail(setup_info.num_tai, sizeof(*setup_info.tai_support));

  for (int i = 0; i < setup_info.num_tai; i++) {
    xnap_tai_support_t *tai = &setup_info.tai_support[i];
    tai->tac = cnf->tac;
    tai->num_plmn = cnf->num_plmn;
    tai->plmn_support = calloc_or_fail(tai->num_plmn, sizeof(*tai->plmn_support));

    for (int j = 0; j < tai->num_plmn; j++) {
      const ngap_plmn_t *src_plmn = &cnf->plmn[j];
      xnap_plmn_support_t *plmn_support = &tai->plmn_support[j];

      plmn_support->plmn = src_plmn->plmn;
      plmn_support->num_nssai = src_plmn->num_nssai;

      if (plmn_support->num_nssai > 0) {
      plmn_support->nssai = calloc_or_fail(plmn_support->num_nssai, sizeof(*plmn_support->nssai));
      memcpy(plmn_support->nssai, src_plmn->s_nssai, plmn_support->num_nssai * sizeof(*plmn_support->nssai));
      }
    }
  }

  setup_info.num_amf_regions = cnf->num_amf_regions;
  if (setup_info.num_amf_regions > 0) {
    setup_info.amf_region_info = calloc_or_fail(cnf->num_amf_regions, sizeof(*setup_info.amf_region_info));
    memcpy(setup_info.amf_region_info, cnf->amf_region_info, cnf->num_amf_regions * sizeof(*setup_info.amf_region_info));
  }

  return setup_info;
}

void gNB_app_register_xn(instance_t instance, ngap_register_gnb_cnf_t *cnf)
{
  MessageDef *msg_p = itti_alloc_new_message(TASK_GNB_APP, 0, XNAP_REGISTER_GNB_REQ);
  xnap_register_gnb_req_t *msg = &XNAP_REGISTER_GNB_REQ(msg_p);
  msg->net_config = read_ip_config_xn(instance);
  msg->ng_setup_info = read_ng_setup_info(cnf, instance);
  LOG_I(GNB_APP,
        "[gNB %ld] Sending XNAP_REGISTER_GNB_REQ for gNB ID %u with %d candidate(s)\n",
        instance,
        msg->ng_setup_info.gNB_id,
        msg->net_config.nb_of_candidate_gNBs);
  itti_send_msg_to_task(TASK_XNAP, GNB_MODULE_ID_TO_INSTANCE(instance), msg_p);

  MessageDef *xn_msg = itti_alloc_new_message(TASK_RRC_GNB, instance, XNAP_F1_SETUP_DONE_IND);
  XNAP_F1_SETUP_DONE_IND(xn_msg).gNB_DU_id = msg->ng_setup_info.gNB_id;
  itti_send_msg_to_task(TASK_XNAP, instance, xn_msg);
}

void *gNB_app_task(void *args_p)
{
  UNUSED(args_p);
  MessageDef *msg_p = NULL;
  const char *msg_name = NULL;
  instance_t instance;
  int result;
  /* for no gcc warnings */
  (void)instance;

  itti_mark_task_ready(TASK_GNB_APP);
  do {
    itti_receive_msg(TASK_GNB_APP, &msg_p);
    msg_name = ITTI_MSG_NAME(msg_p);
    instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
    switch (ITTI_MSG_ID(msg_p)) {
      case TERMINATE_MESSAGE:
        LOG_W(GNB_APP, " *** Exiting GNB_APP thread\n");
        itti_exit_task();
        break;

      case NGAP_REGISTER_GNB_CNF:
        LOG_I(GNB_APP,
              "[gNB %ld] Received %s: associated AMF %d\n",
              instance,
              msg_name,
              NGAP_REGISTER_GNB_CNF(msg_p).nb_amf);
        LOG_I(GNB_APP, "[gNB %ld] Starting Xn procedure sequence\n", instance);
        gNB_app_register_xn(instance, &NGAP_REGISTER_GNB_CNF(msg_p));
        break;

      default:
        LOG_E(GNB_APP, "Received unexpected message %s\n", msg_name);
        break;
    }
    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
  } while (1);
  return NULL;
}

void *rrc_gnb_task(void *args_p)
{
  UNUSED(args_p);
  MessageDef *msg_p;
  instance_t instance;
  int result;
  static int nb_done = 0;

  itti_mark_task_ready(TASK_RRC_GNB);
  LOG_I(NR_RRC, "Entering main loop of NR_RRC message task\n");

  while (1) {
    itti_receive_msg(TASK_RRC_GNB, &msg_p);
    const char *msg_name_p = ITTI_MSG_NAME(msg_p);
    instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
    LOG_D(NR_RRC,
          "RRC GNB Task Received %s for instance %ld from task %s\n",
          ITTI_MSG_NAME(msg_p),
          instance,
          ITTI_MSG_ORIGIN_NAME(msg_p));

    switch (ITTI_MSG_ID(msg_p)) {
      case TERMINATE_MESSAGE:
        LOG_W(NR_RRC, " *** Exiting NR_RRC thread\n");
        itti_exit_task();
        break;

      case XNAP_SETUP_IND:
        nb_done++;
        LOG_I(NR_RRC, "[gNB %ld] Received Xn setup indication %d / %d for %d instance(s))\n", instance, nb_done, nb_gnb_inst * (nb_gnb_inst -1), nb_gnb_inst);
        if (nb_done >= (nb_gnb_inst * (nb_gnb_inst - 1))) {
          LOG_I(NR_RRC, "Xn setup flow completed successfully for all %d gNB instance(s)!\n", nb_gnb_inst);
          exit(EXIT_SUCCESS);
        }
        break;

      case XNAP_PEER_SHUTDOWN_IND:
        nb_done++;
        LOG_I(NR_RRC,
              "[gNB %ld] Received Xn peer shutdown indications %d of %d instance(s) done)\n",
              instance, nb_done, nb_gnb_inst);
        if (nb_done >= nb_gnb_inst) {
          LOG_I(NR_RRC, "Xn setup flow completed successfully for all %d gNB instance(s)!\n", nb_gnb_inst);
          exit(EXIT_SUCCESS);
        }
        break;

      default:
        LOG_E(NR_RRC, "[gNB %ld] Received unexpected message %s\n", instance, msg_name_p);
        break;
    }
    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    msg_p = NULL;
  }
}

static int extract_conf_files(int argc, char **argv, char **Ofiles, int max_files)
{
  int n = 0;
  for (int i = 0; i < argc - 1; i++) {
    if (strlen(argv[i]) >= 2 && argv[i][1] == 'O') {
      if (n >= max_files) {
        fprintf(stderr, "[XN-SIM] warning: more than %d -O files given, ignoring \"%s\"\n", max_files, argv[i + 1]);
        continue;
      }
      Ofiles[n++] = argv[i + 1];
    }
  }
  return n;
}

int main(int argc, char **argv)
{
  /* --- collect every -O file from the real command line -------------- */
  char *Ofiles[8];
  int nOfiles = extract_conf_files(argc, argv, Ofiles, sizeofArray(Ofiles));
  nb_gnb_inst = (nOfiles > 0) ? nOfiles : 1;

  gnb_id_tab = calloc_or_fail(nb_gnb_inst, sizeof(*gnb_id_tab));
  tac_tab = calloc_or_fail(nb_gnb_inst, sizeof(*tac_tab));
  mcc_tab = calloc_or_fail(nb_gnb_inst, sizeof(*mcc_tab));
  mnc_tab = calloc_or_fail(nb_gnb_inst, sizeof(*mnc_tab));
  mnc_len_tab = calloc_or_fail(nb_gnb_inst, sizeof(*mnc_len_tab));
  xn_net_config_tab = calloc_or_fail(nb_gnb_inst, sizeof(*xn_net_config_tab));

  char **synth_argv = calloc_or_fail(3, sizeof(*synth_argv));
  int synth_argc;
  if (nOfiles > 0) {
    synth_argv[0] = argv[0];
    synth_argv[1] = "-O";
    synth_argv[2] = Ofiles[0];
    synth_argc = 3;
  }

  uniqCfg = load_configmodule(nOfiles > 0 ? synth_argc : argc, nOfiles > 0 ? synth_argv : argv, CONFIG_ENABLECMDLINEONLY);
  if (uniqCfg == NULL) {
    exit_fun("[SOFTMODEM] Error, configuration module init failed\n");
  }

  logInit();
  set_softmodem_sighandler();

  LOG_I(GNB_APP, "%d gNB instance(s) / config file(s) for the Xn simulator\n", nb_gnb_inst);

  /* Initialize ITTI tasks before we start sending any registration
   * message, exactly as in the single-instance version. */
  itti_init(TASK_MAX, tasks_info);
  int rc;
  rc = itti_create_task(TASK_SCTP, sctp_eNB_task, NULL);
  AssertFatal(rc >= 0, "Create task for SCTP failed\n");
  rc = itti_create_task(TASK_NGAP, ngap_gNB_task, NULL);
  AssertFatal(rc >= 0, "Create task for NGAP failed\n");
  rc = itti_create_task(TASK_XNAP, xnap_task, NULL);
  AssertFatal(rc >= 0, "Create task for XNAP failed\n");
  rc = itti_create_task(TASK_RRC_GNB, rrc_gnb_task, NULL);
  AssertFatal(rc >= 0, "Create task for RRC failed\n");
  rc = itti_create_task(TASK_GNB_APP, gNB_app_task, NULL);
  AssertFatal(rc >= 0, "Create task for GNB APP failed\n");

  /* --- load, extract, unload each file in turn - fully independent,
   * never merged, never more than one file open at a time. File 0's
   * config is already loaded above, so this loop does NOT reload it. */
  for (int i = 0; i < nb_gnb_inst; i++) {
    if (i > 0) {
      char **this_argv = calloc_or_fail(3, sizeof(*this_argv));
      this_argv[0] = argv[0];
      this_argv[1] = "-O";
      this_argv[2] = Ofiles[i];
      uniqCfg = load_configmodule(3, this_argv, CONFIG_ENABLECMDLINEONLY);
      if (uniqCfg == NULL) {
        exit_fun("[SOFTMODEM] Error, configuration module init failed\n");
      }
    }

    MessageDef *msg_p = itti_alloc_new_message(TASK_GNB_APP, 0, NGAP_REGISTER_GNB_REQ);
    RCconfig_NR_NG(msg_p, 0);

    gnb_id_tab[i] = NGAP_REGISTER_GNB_REQ(msg_p).gNB_id;
    tac_tab[i] = NGAP_REGISTER_GNB_REQ(msg_p).tac;
    mcc_tab[i] = NGAP_REGISTER_GNB_REQ(msg_p).plmn[0].plmn.mcc;
    mnc_tab[i] = NGAP_REGISTER_GNB_REQ(msg_p).plmn[0].plmn.mnc;
    mnc_len_tab[i] = NGAP_REGISTER_GNB_REQ(msg_p).plmn[0].plmn.mnc_digit_length;
    xn_net_config_tab[i] = read_ip_config_xn_from_loaded_config(0);

    if (i == 0) {
      gnb_id = gnb_id_tab[0];
      tac = tac_tab[0];
      mcc = mcc_tab[0];
      mnc = mnc_tab[0];
      mnc_len = mnc_len_tab[0];
    }

    LOG_I(GNB_APP,
          "Sending NGAP_REGISTER_GNB_REQ to TASK_NGAP for gNB instance %d (gNB ID %u, file %s)\n",
          i,
          gnb_id_tab[i],
          (nOfiles > 0) ? Ofiles[i] : "(default)");
    itti_send_msg_to_task(TASK_NGAP, i, msg_p);

    if (i < nb_gnb_inst - 1) {
      end_configmodule(uniqCfg);
      uniqCfg = NULL;
    }
  }

  printf("TYPE <CTRL-C> TO TERMINATE\n");
  itti_wait_tasks_end(NULL);
  logClean();
  printf("Bye.\n");
  return 0;
}
