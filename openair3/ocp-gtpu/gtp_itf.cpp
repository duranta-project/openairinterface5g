/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <map>
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#include "common/platform_types.h"
#include "common/utils/system.h"
#include <openair3/UTILS/conversions.h>
#include "common/utils/LOG/log.h"
#include <common/utils/ocp_itti/intertask_interface.h>
#include "sim.h"

// TODO these dependencies should not exist and be removed
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "openair2/LAYER2/RLC/rlc.h"

#include "gtp_itf.h"
#include "gtpu_extensions.h"

#pragma pack(1)

typedef struct Gtpv1uMsgHeader {
  uint8_t PN: 1;
  uint8_t S: 1;
  uint8_t E: 1;
  uint8_t spare: 1;
  uint8_t PT: 1;
  uint8_t version: 3;
  uint8_t msgType;
  uint16_t msgLength;
  teid_t teid;
} __attribute__((packed)) Gtpv1uMsgHeaderT;

// TS 38.425, Figure 5.5.2.2-1
typedef struct DlDataDeliveryStatus_flags {
  uint8_t LPR: 1; // Lost packet report
  uint8_t FFI: 1; // Final Frame Ind
  uint8_t deliveredPdcpSn: 1; // Highest Delivered NR PDCP SN Ind
  uint8_t transmittedPdcpSn: 1; // Highest Transmitted NR PDCP SN Ind
  uint8_t pduType: 4; // PDU type
  uint8_t CR: 1; // Cause Report
  uint8_t deliveredReTxPdcpSn: 1; // Delivered retransmitted NR PDCP SN Ind
  uint8_t reTxPdcpSn: 1; // Retransmitted NR PDCP SN Ind
  uint8_t DRI: 1; // Data Rate Indication
  uint8_t deliveredPdcpSnRange: 1; // Delivered NR PDCP SN Range Ind
  uint8_t spare: 3;
  uint32_t drbBufferSize; // Desired buffer size for the data radio bearer
} __attribute__((packed)) DlDataDeliveryStatus_flagsT;

typedef struct Gtpv1uMsgHeaderOptFields {
  uint8_t seqNum1Oct;
  uint8_t seqNum2Oct;
  uint8_t NPDUNum;
  uint8_t NextExtHeaderType;
} __attribute__((packed)) Gtpv1uMsgHeaderOptFieldsT;

#define DL_PDU_SESSION_INFORMATION 0
#define UL_PDU_SESSION_INFORMATION 1

typedef struct PDUSessionContainer {
  uint8_t spare: 4;
  uint8_t PDU_type: 4;
  uint8_t QFI: 6;
  uint8_t Reflective_QoS_activation: 1;
  uint8_t Paging_Policy_Indicator: 1;
} __attribute__((packed)) PDUSessionContainerT;

typedef struct Gtpv1uExtHeader {
  uint8_t ExtHeaderLen;
  PDUSessionContainerT pdusession_cntr;
  uint8_t NextExtHeaderType;
} __attribute__((packed)) Gtpv1uExtHeaderT;

  typedef struct Gtpv1Error {
    Gtpv1uMsgHeaderT h;
    uint8_t teid_data_i;
    teid_t teid;
    uint8_t addr_data_i;
    uint16_t addr_len;
  } __attribute__((packed)) Gtpv1uError;

#pragma pack()

// TS 29.281, fig 5.2.1-3
#define PDU_SESSION_CONTAINER (0x85)
#define NR_RAN_CONTAINER (0x84)

// TS 29.281, 5.2.1
#define EXT_HDR_LNTH_OCTET_UNITS (4)
#define NO_MORE_EXT_HDRS (0)

// TS 29.060, table 7.1 defines the possible message types
// here are all the possible messages (3GPP R16)
#define GTP_ECHO_REQ (1)
#define GTP_ECHO_RSP (2)
#define GTP_ERROR_INDICATION (26)
#define GTP_SUPPORTED_EXTENSION_HEADER_INDICATION (31)
#define GTP_END_MARKER (254)
#define GTP_GPDU (255)

/** NO_QFI: indicates no QFI marking (F1-U tunnel or N3-U tunnel with no SDAP header)
 * Used when there is no UL PDU Session Information (SDAP header) present */
#define NO_QFI (-1)

/** number of packets to receive at once in recvmmsg() */
#define VLEN 8
/** buffer size for each packet */
#define BUFSIZE 65536

/** GTP bearer context: for sending data */
typedef struct gtpv1u_bearer_s {
  int sock_fd;
  struct sockaddr_storage ip;
  teid_t teid_incoming;
  teid_t teid_outgoing;
  uint16_t seqNum;
  uint8_t npduNum;
  int32_t nru_sequence_number;
} gtpv1u_bearer_t;

typedef struct {
  map<ue_id_t, gtpv1u_bearer_t> bearers;
} teidData_t;

typedef struct {
  ue_id_t ue_id;
  /** Incoming TEID mapping key:
   *  - F1-U: DRB ID (direct TEID-to-DRB routing on non-SDAP callback path)
   *  - N3-U: PDU session ID (TEID-to-PDU session; SDAP callback then resolves QFI-to-DRB) */
  uint16_t incoming_rb_id;
  gtpCallback callBack;
  teid_t outgoing_teid;
  gtpCallbackSDAP callBackSDAP;
  /** PDU Session ID (1..255) */
  uint16_t pdusession_id;
} ueidData_t;

typedef struct {
  int h;
  pthread_t t;
} gtpThread_t;

class gtpEndPoint {
 public:
  openAddr_t addr;
  uint8_t foundAddr[20];
  int foundAddrLen;
  int ipVersion;
  gtpThread_t thrData;
  map<uint64_t, teidData_t> ue2te_mapping;
  // we use the same port number for source and destination address
  // this allow using non standard gtp port number (different from 2152)
  // and so, for example tu run 4G and 5G cores on one system
  tcp_udp_port_t get_dstport()
  {
    return (tcp_udp_port_t)atol(addr.destinationService);
  }
};

static void gtpv1uReceiverCancel(pthread_t t);
class gtpEndPoints {
 public:
  pthread_mutex_t gtp_lock = PTHREAD_MUTEX_INITIALIZER;
  // the instance id will be the Linux socket handler, as this is uniq
  map<uint64_t, gtpEndPoint> instances;
  map<uint64_t, ueidData_t> te2ue_mapping;
  gtpEndPoints()
  {
    unsigned int seed;
    fill_random(&seed, sizeof(seed));
    srandom(seed);
  }

  ~gtpEndPoints()
  {
    // automatically close all sockets on quit
    for (const auto &p : instances) {
      gtpv1uReceiverCancel(p.second.thrData.t);
      close(p.first);
    }
  }
};

static gtpEndPoints globGtp;

// note TEid 0 is reserved for specific usage: echo req/resp, error and supported extensions
static teid_t gtpv1uNewTeid(void)
{
#ifdef GTPV1U_LINEAR_TEID_ALLOCATION
  g_gtpv1u_teid = g_gtpv1u_teid + 1;
  return g_gtpv1u_teid;
#else
  return random() + random() % (RAND_MAX - 1) + 1;
#endif
}

instance_t legacyInstanceMapping = 0;
#define compatInst(a) ((a) == 0 || (a) == INSTANCE_DEFAULT ? legacyInstanceMapping : a)

#define getInstRetVoid(insT)                                 \
  auto instChk = globGtp.instances.find(compatInst(insT));   \
  if (instChk == globGtp.instances.end()) {                  \
    LOG_E(GTPU, "try to get a gtp-u not existing output\n"); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                 \
    return;                                                  \
  }                                                          \
  gtpEndPoint *inst = &instChk->second;

#define getInstRetInt(insT)                                  \
  auto instChk = globGtp.instances.find(compatInst(insT));   \
  if (instChk == globGtp.instances.end()) {                  \
    LOG_E(GTPU, "try to get a gtp-u not existing output\n"); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                 \
    return GTPNOK;                                           \
  }                                                          \
  gtpEndPoint *inst = &instChk->second;

#define getUeRetVoid(insT, Ue)                                                                                    \
  auto ptrUe = insT->ue2te_mapping.find(Ue);                                                                      \
                                                                                                                  \
  if (ptrUe == insT->ue2te_mapping.end()) {                                                                       \
    LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, ue_id); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                                                                      \
    return;                                                                                                       \
  }

#define getUeRetInt(insT, Ue)                                                                                     \
  auto ptrUe = insT->ue2te_mapping.find(Ue);                                                                      \
                                                                                                                  \
  if (ptrUe == insT->ue2te_mapping.end()) {                                                                       \
    LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, ue_id); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                                                                      \
    return GTPNOK;                                                                                                \
  }

#define HDR_MAX 256 // 256 is supposed to be larger than any gtp header
static int gtpv1uCreateAndSendMsg(gtpv1u_bearer_t *bearer,
                                  int msgType,
                                  uint8_t *Msg,
                                  int msgLen,
                                  bool seqNumFlag,
                                  bool npduNumFlag,
                                  gtpu_extension_header_t *extensions,
                                  int extensions_count)
{
  uint8_t header[HDR_MAX];
  Gtpv1uMsgHeaderT *msgHdr = (Gtpv1uMsgHeaderT *)header;
  // N should be 0 for us (it was used only in 2G and 3G)
  msgHdr->PN = npduNumFlag;
  msgHdr->S = seqNumFlag;
  msgHdr->E = extensions_count != 0;
  msgHdr->spare = 0;
  // PT=0 is for GTP' TS 32.295 (charging)
  msgHdr->PT = 1;
  msgHdr->version = 1;
  msgHdr->msgType = msgType;
  msgHdr->teid = htonl(bearer->teid_outgoing);

  uint8_t *curPtr = header + sizeof(Gtpv1uMsgHeaderT);
  if (msgHdr->PN || msgHdr->S || msgHdr->E) {
    *(uint16_t *)curPtr = seqNumFlag ? bearer->seqNum : 0x0000;
    curPtr += sizeof(uint16_t);
    *(uint8_t *)curPtr = npduNumFlag ? bearer->npduNum : 0x00;
    curPtr++;
    *curPtr = extensions_count ? serialize_gtpu_extension_type(extensions[0].type) : 0;
    curPtr++;
  }

  for (int i = 0; i < extensions_count; i++) {
    int available_size = sizeof(header) - (curPtr - header);
    gtpu_extension_header_type_t next = i == extensions_count - 1 ? GTPU_EXT_NONE : extensions[i + 1].type;
    int len = serialize_extension(&extensions[i], next, curPtr, available_size);
    if (len == -1) {
      LOG_E(GTPU, "GTP extension serialization: buffer too small\n");
      return GTPNOK;
    }
    curPtr += len;
  }

  size_t hdr_len = curPtr - header;
  msgHdr->msgLength = htons(hdr_len - sizeof(Gtpv1uMsgHeaderT) + msgLen);

  // Fix me: add IPv6 support
  DevAssert(bearer->ip.ss_family == AF_INET);
  struct sockaddr_in *to = (struct sockaddr_in *)&bearer->ip;
  LOG_D(GTPU,
        "Peer IP:" IPV4_ADDR " port:%u outgoing TEID:0x%x\n",
        IPV4_ADDR_FORMAT(to->sin_addr.s_addr),
        htons(to->sin_port),
        bearer->teid_outgoing);

  struct iovec iov[2] = {
      { .iov_base = msgHdr, .iov_len = hdr_len, },
      { .iov_base = Msg, .iov_len = (size_t) msgLen, },
  };
  struct msghdr m = {
    .msg_name = to,
    .msg_namelen = sizeof(*to),
    .msg_iov = iov,
    .msg_iovlen = Msg ? 2U : 1U,
  };
  ssize_t ret = sendmsg(bearer->sock_fd, &m, 0);
  if (ret != (ssize_t) (hdr_len + msgLen)) {
    LOG_E(GTPU, "[SD %d] Failed to send data, ret: %ld, errno: %d\n", bearer->sock_fd, ret, errno);
    return GTPNOK;
  }

  return !GTPNOK;
}

/** Internal function to send GTP-U packet with optional QFI marking
 * Per TS 29.281 §5.2, QFI is carried in PDU Session Container extension header for N3-U
 * @param qfi QoS Flow Identifier (0..63) for N3-U, or NO_QFI (-1) for F1-U */
static void _gtpv1uSendDirect(instance_t instance,
                              ue_id_t ue_id,
                              int bearer_id,
                              int qfi,
                              uint8_t *buf,
                              size_t len,
                              bool seqNumFlag,
                              bool npduNumFlag,
                              int32_t nru_seqnum)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2 = ptrUe->second.bearers.find(bearer_id);

  if (ptr2 == ptrUe->second.bearers.end()) {
    LOG_E(GTPU, "[%ld] GTP-U instance: sending a packet to a non existant UE:RAB: %lx/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  LOG_D(GTPU,
        "[%ld] sending a packet to UE:RAB:TEID %lx/%d/0x%x, len %lu, oldseq %d, oldnum %d\n",
        instance,
        ue_id,
        bearer_id,
        ptr2->second.teid_outgoing,
        len,
        ptr2->second.seqNum,
        ptr2->second.npduNum);

  if (seqNumFlag)
    ptr2->second.seqNum++;

  if (npduNumFlag)
    ptr2->second.npduNum++;

  // copy to release the mutex
  gtpv1u_bearer_t bearer = ptr2->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  int extension_count = 0;
  gtpu_extension_header_t ext[2];
  /** Add PDU Session Container extension header if QFI is present (N3-U tunnel)
   * Per TS 29.281 Figure 5.2.1-3 note 4, PDU Session Container must be the first Extension Header
   * Per TS 29.281 §5.2, QFI is carried in UL PDU Session Information IE for N3-U */
  if (qfi != NO_QFI) {
    ext[extension_count] = {
      .type = GTPU_EXT_UL_PDU_SESSION_INFORMATION,
      .ul_pdu_session_information = {
        .qmp = false,
        .dl_delay_ind = false,
        .ul_delay_ind = false,
        .snp = false,
        .n3n9_delay_ind = false,
        .new_ie_flag = false,
        .qfi = qfi,
      }
    };
    extension_count++;
    LOG_D(GTPU,
          "UL TX: Adding PDU Session Container with QFI=%d (ue=%ld bearer_id=%d outgoing_teid=0x%x)\n",
          qfi,
          ue_id,
          bearer_id,
          bearer.teid_outgoing);
  }

  if (nru_seqnum != -1) {
    ext[extension_count] = {
      .type = GTPU_EXT_DL_USER_DATA,
      .dl_user_data = {
        .dl_discard_blocks = false,
        .dl_flush = false,
        .report_polling = false,
        .request_out_of_seq_report = false,
        .report_delivered = false,
        .user_data_existence_flag = false,
        .assistance_info_report_polling_flag = false,
        .retransmission_flag = false,
        .nru_sequence_number = (uint32_t)nru_seqnum
      }
    };
    extension_count++;
  }

  DevAssert(compatInst(instance) == bearer.sock_fd);
  gtpv1uCreateAndSendMsg(&bearer,
                         GTP_GPDU,
                         buf,
                         len,
                         seqNumFlag,
                         npduNumFlag,
                         ext,
                         extension_count);
}

/** Send GTP-U packet with QFI marking for N3-U tunnel
 * Per TS 29.281 §5.2, QFI is carried in PDU Session Container extension header
 * Used by SDAP layer when forwarding UL packets to N3-U tunnel
 * @param qfi QoS Flow Identifier (0..63) extracted from SDAP header */
void gtpv1uSendDirectWithQFI(instance_t instance, ue_id_t ue_id, int bearer_id, int qfi, uint8_t *buf, size_t len)
{
  AssertFatal(qfi >= 0 && qfi < MAX_QOS_FLOWS,
              "Invalid QFI %d for gtpv1uSendDirectWithQFI (expected 0..%d)\n",
              qfi,
              MAX_QOS_FLOWS - 1);
  _gtpv1uSendDirect(instance, ue_id, bearer_id, qfi, buf, len, false, false, -1);
}

/** Send GTP-U packet with no QFI marking for F1-U tunnel
 * @note qfi is set to NO_QFI (-1) for F1-U */
void gtpv1uSendDirect(instance_t instance,
                      ue_id_t ue_id,
                      int bearer_id,
                      uint8_t *buf,
                      size_t len,
                      bool seqNumFlag,
                      bool npduNumFlag)
{
  _gtpv1uSendDirect(instance, ue_id, bearer_id, NO_QFI, buf, len, seqNumFlag, npduNumFlag, -1);
}

void gtpv1uSendDirectWithNRUSeqNum(instance_t instance,
                                   ue_id_t ue_id,
                                   int bearer_id,
                                   uint8_t *buf,
                                   size_t len)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);
  auto ptr2 = ptrUe->second.bearers.find(bearer_id);

  if (ptr2 == ptrUe->second.bearers.end()) {
    LOG_E(GTPU, "[%ld] GTP-U instance: sending a packet to a non existant UE:RAB: %lx/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  int32_t nru_seqnum = ptr2->second.nru_sequence_number;
  ptr2->second.nru_sequence_number++;
  ptr2->second.nru_sequence_number &= (1 << 24) - 1;

  pthread_mutex_unlock(&globGtp.gtp_lock);

  _gtpv1uSendDirect(instance, ue_id, bearer_id, NO_QFI, buf, len, false, false, nru_seqnum);
}

static void fillDlDeliveryStatusReport(gtpu_extension_header_t *ext, uint32_t RLC_buffer_availability, uint32_t NR_PDCP_PDU_SN)
{
  *ext = {
    .type = GTPU_EXT_DL_DATA_DELIVERY_STATUS,
    .dl_data_delivery_status = {
      /* previous version of the code was sending highest_transmitted_nr_pdcp_sn if
       * it is != 0, let's do the same for the moment */
      .highest_transmitted_nr_pdcp_sn_ind = NR_PDCP_PDU_SN != 0,
      .highest_delivered_nr_pdcp_sn_ind = false,
      .final_frame_ind = false,
      .lost_packet_report = false,
      .delivered_nr_pdcp_sn_range_ind = false,
      .data_rate_ind = false,
      .retransmitted_nr_pdcp_sn_ind = false,
      .delivered_retransmitted_nr_pdcp_ind = false,
      .cause_report = false,
      .desired_buffer_size = RLC_buffer_availability,
      .highest_transmitted_nr_pdcp_sn = NR_PDCP_PDU_SN
    }
  };
}

static void gtpv1uEndTunnel(instance_t instance, gtpv1u_enb_end_marker_req_t *req)
{
  ue_id_t ue_id = req->rnti;
  int bearer_id = req->rab_id;
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2 = ptrUe->second.bearers.find(bearer_id);

  if (ptr2 == ptrUe->second.bearers.end()) {
    LOG_E(GTPU, "[%ld] GTP-U sending a packet to a non existant UE:RAB: %lx/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  LOG_D(GTPU,
        "[%ld] sending a end packet packet to UE:RAB:TEID %lx/%d/0x%x\n",
        instance,
        ue_id,
        bearer_id,
        ptr2->second.teid_outgoing);
  gtpv1u_bearer_t tmp = ptr2->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);
  Gtpv1uMsgHeaderT msgHdr;
  // N should be 0 for us (it was used only in 2G and 3G)
  msgHdr.PN = 0;
  msgHdr.S = 0;
  msgHdr.E = 0;
  msgHdr.spare = 0;
  // PT=0 is for GTP' TS 32.295 (charging)
  msgHdr.PT = 1;
  msgHdr.version = 1;
  msgHdr.msgType = GTP_END_MARKER;
  msgHdr.msgLength = htons(0);
  msgHdr.teid = htonl(tmp.teid_outgoing);

  // Fix me: add IPv6 support
  DevAssert(instance == tmp.sock_fd);
  DevAssert(tmp.ip.ss_family == AF_INET);
  struct sockaddr_in *to = (struct sockaddr_in *)&tmp.ip;
  LOG_D(GTPU, "[%ld] sending end packet to " IPV4_ADDR " port %d\n", instance, IPV4_ADDR_FORMAT(to->sin_addr.s_addr), htons(to->sin_port));

  ssize_t ret = sendto(tmp.sock_fd, &msgHdr, sizeof(msgHdr), 0, (struct sockaddr *)to, sizeof(*to));
  if (ret != sizeof(msgHdr)) {
    LOG_E(GTPU, "[%d] Failed to send data with buffer size %lu: ret %ld errno %d\n", tmp.sock_fd, sizeof(msgHdr), ret, errno);
  }
}

static int udpServerSocket(openAddr_s addr)
{
  LOG_I(GTPU, "Initializing UDP for local address %s with port %s\n", addr.originHost, addr.originService);
  int status;
  struct addrinfo hints = {0}, *servinfo, *p;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(addr.originHost, addr.originService, &hints, &servinfo)) != 0) {
    LOG_E(GTPU, "getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  int sockfd = -1;

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      LOG_W(GTPU, "socket: %s\n", strerror(errno));
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      LOG_W(GTPU, "bind: %s\n", strerror(errno));
      continue;
    } else {
      // We create the gtp instance on the socket
      globGtp.instances[sockfd].addr = addr;

      if (p->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        memcpy(globGtp.instances[sockfd].foundAddr, &ipv4->sin_addr.s_addr, sizeof(ipv4->sin_addr.s_addr));
        globGtp.instances[sockfd].foundAddrLen = sizeof(ipv4->sin_addr.s_addr);
        globGtp.instances[sockfd].ipVersion = 4;
        break;
      } else if (p->ai_family == AF_INET6) {
        LOG_W(GTPU, "Local address is IP v6\n");
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
        memcpy(globGtp.instances[sockfd].foundAddr, &ipv6->sin6_addr.s6_addr, sizeof(ipv6->sin6_addr.s6_addr));
        globGtp.instances[sockfd].foundAddrLen = sizeof(ipv6->sin6_addr.s6_addr);
        globGtp.instances[sockfd].ipVersion = 6;
      } else
        AssertFatal(false, "Local address is not IPv4 or IPv6");
    }

    break; // if we get here, we must have connected successfully
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL) {
    // looped off the end of the list with no successful bind
    LOG_E(GTPU, "failed to bind socket: %s %s \n", addr.originHost, addr.originService);
    return -1;
  }

  int sendbuff = 1000 * 1000 * 10;
  AssertFatal(0 == setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)), "%s", strerror(errno));
  LOG_D(GTPU,
        "[%d] Created listener for paquets to: %s:%s, send buffer size: %d\n",
        sockfd,
        addr.originHost,
        addr.originService,
        sendbuff);
  return sockfd;
}

static void* gtpv1uReceiver(void *thr);
instance_t gtpv1Init(openAddr_t context)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  int id = udpServerSocket(context);

  if (id >= 0) {
    LOG_I(GTPU, "Created gtpu instance id: %d\n", id);
    getInstRetInt(compatInst(id));
    inst->thrData.h = id;
    char name[32];
    snprintf(name, sizeof(name), "GTPrx_%d", id);
    threadCreate(&inst->thrData.t, gtpv1uReceiver, &inst->thrData, name, -1, OAI_PRIORITY_RT);
  } else
    LOG_E(GTPU, "can't create GTP-U instance\n");

  pthread_mutex_unlock(&globGtp.gtp_lock);
  return id;
}

/* \brief remove the GTP instance from the list of instances. Does not make an
 * attempt to free corresponding TEIDs, as we have many and will simply not
 * reuse it later. */
int gtpv1Term(instance_t instance)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  gtpv1uReceiverCancel(inst->thrData.t);
  close(instance);
  globGtp.instances.erase(instance);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  return 0;
}

void GtpuUpdateTunnelOutgoingAddressAndTeid(instance_t instance,
                                            ue_id_t ue_id,
                                            ebi_t bearer_id,
                                            in_addr_t newOutgoingAddr,
                                            teid_t newOutgoingTeid)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2 = ptrUe->second.bearers.find(bearer_id);

  if (ptr2 == ptrUe->second.bearers.end()) {
    LOG_E(GTPU, "[%ld] Update tunnel for a existing ue id %lu, but wrong bearer_id %u\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  struct sockaddr_in *sockaddr = (struct sockaddr_in *)&ptr2->second.ip;
  sockaddr->sin_family = AF_INET;
  memcpy(&sockaddr->sin_addr, &newOutgoingAddr, sizeof(newOutgoingAddr));
  AssertFatal(ptr2->second.ip.ss_family == AF_INET, "only IPv4 is supported\n");
  ptr2->second.teid_outgoing = newOutgoingTeid;
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  struct sockaddr_in *sa4 = (struct sockaddr_in *)sockaddr;
  struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sockaddr;
  LOG_I(GTPU,
        "[%ld] UE ID %ld: Update tunnel TEID incoming 0x%x outgoing 0x%x to remote IPv4 %s, IPv6 %s, port %d\n",
        instance,
        ue_id,
        ptr2->second.teid_incoming,
        ptr2->second.teid_outgoing,
        inet_ntop(AF_INET, &sa4->sin_addr, ip4, INET_ADDRSTRLEN),
        inet_ntop(AF_INET6, &sa6->sin6_addr, ip6, INET6_ADDRSTRLEN),
        ntohs(sa4->sin_port));

  pthread_mutex_unlock(&globGtp.gtp_lock);
  return;
}

teid_t newGtpuCreateTunnel(instance_t instance,
                           ue_id_t ue_id,
                           int incoming_bearer_id,
                           int outgoing_bearer_id,
                           teid_t outgoing_teid,
                           transport_layer_addr_t remoteAddr,
                           gtpCallback callBack,
                           gtpCallbackSDAP callBackSDAP)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  auto it = inst->ue2te_mapping.find(ue_id);

  if (it != inst->ue2te_mapping.end() && it->second.bearers.find(outgoing_bearer_id) != it->second.bearers.end()) {
    LOG_W(GTPU, "[%ld] Create a config for a already existing GTP tunnel (ue id %lu)\n", instance, ue_id);
    inst->ue2te_mapping.erase(it);
  }

  teid_t incoming_teid = gtpv1uNewTeid();

  while (globGtp.te2ue_mapping.find(incoming_teid) != globGtp.te2ue_mapping.end()) {
    LOG_W(GTPU, "[%ld] generated a random TEID that exists, re-generating (0x%x)\n", instance, incoming_teid);
    incoming_teid = gtpv1uNewTeid();
  };

  globGtp.te2ue_mapping[incoming_teid].ue_id = ue_id;
  globGtp.te2ue_mapping[incoming_teid].incoming_rb_id = incoming_bearer_id;
  globGtp.te2ue_mapping[incoming_teid].outgoing_teid = outgoing_teid;
  globGtp.te2ue_mapping[incoming_teid].callBack = callBack;
  globGtp.te2ue_mapping[incoming_teid].callBackSDAP = callBackSDAP;
  globGtp.te2ue_mapping[incoming_teid].pdusession_id = (uint8_t)outgoing_bearer_id;

  gtpv1u_bearer_t bearer = {
    .sock_fd = (int) compatInst(instance), // avoid warning on narrowing conversion: instance is long, sock_fd is int
    .teid_incoming = incoming_teid,
    .teid_outgoing = outgoing_teid,
  };

  int addrs_length_in_bytes = remoteAddr.length / 8;
  struct sockaddr_in *sa4 = (struct sockaddr_in *)&bearer.ip;
  struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&bearer.ip;
  switch (addrs_length_in_bytes) {
    case 4:
      memcpy(&sa4->sin_addr, remoteAddr.buffer, 4);
      sa4->sin_family = AF_INET;
      sa4->sin_port = htons(inst->get_dstport());
      break;

    case 16:
      AssertFatal(false, "IPv6 not supported\n");
      break;

    case 20:
      AssertFatal(false, "dual-IPv4/v6 not supported\n");
      break;

    default:
      AssertFatal(false, "SGW Address size impossible");
  }

  inst->ue2te_mapping[ue_id].bearers[outgoing_bearer_id] = bearer;
  pthread_mutex_unlock(&globGtp.gtp_lock);
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  LOG_I(GTPU,
        "[%ld] UE ID %ld: Create tunnel TEID incoming 0x%x outgoing 0x%x to remote IPv4 %s, IPv6 %s, port %d\n",
        instance,
        ue_id,
        bearer.teid_incoming,
        bearer.teid_outgoing,
        inet_ntop(AF_INET, &sa4->sin_addr, ip4, INET_ADDRSTRLEN),
        inet_ntop(AF_INET6, &sa6->sin6_addr, ip6, INET6_ADDRSTRLEN),
        ntohs(sa4->sin_port));
  return incoming_teid;
}

int gtpv1u_create_s1u_tunnel(instance_t instance,
                             const gtpv1u_enb_create_tunnel_req_t *create_tunnel_req,
                             gtpv1u_enb_create_tunnel_resp_t *create_tunnel_resp,
                             gtpCallback callBack)
{
  LOG_D(GTPU,
        "[%ld] Start create tunnels for UE ID %u, num_tunnels %d, sgw_S1u_teid %x\n",
        instance,
        create_tunnel_req->rnti,
        create_tunnel_req->num_tunnels,
        create_tunnel_req->sgw_S1u_teid[0]);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));

  uint8_t addr[inst->foundAddrLen];
  memcpy(addr, inst->foundAddr, inst->foundAddrLen);
  pthread_mutex_unlock(&globGtp.gtp_lock);

  for (int i = 0; i < create_tunnel_req->num_tunnels; i++) {
    AssertFatal(create_tunnel_req->eps_bearer_id[i] > 4,
                "From legacy code not clear, seems impossible (bearer=%d)\n",
                create_tunnel_req->eps_bearer_id[i]);
    int incoming_rb_id = create_tunnel_req->eps_bearer_id[i] - 4;
    teid_t teid = newGtpuCreateTunnel(compatInst(instance),
                                      create_tunnel_req->rnti,
                                      incoming_rb_id,
                                      create_tunnel_req->eps_bearer_id[i],
                                      create_tunnel_req->sgw_S1u_teid[i],
                                      create_tunnel_req->sgw_addr[i],
                                      callBack,
                                      NULL);
    create_tunnel_resp->status = 0;
    create_tunnel_resp->rnti = create_tunnel_req->rnti;
    create_tunnel_resp->num_tunnels = create_tunnel_req->num_tunnels;
    create_tunnel_resp->enb_S1u_teid[i] = teid;
    create_tunnel_resp->eps_bearer_id[i] = create_tunnel_req->eps_bearer_id[i];
    memcpy(create_tunnel_resp->enb_addr.buffer, addr, sizeof(addr));
    create_tunnel_resp->enb_addr.length = sizeof(addr);
  }

  return !GTPNOK;
}

int gtpv1u_update_s1u_tunnel(const instance_t instance,
                             const gtpv1u_enb_create_tunnel_req_t *const create_tunnel_req,
                             const rnti_t prior_rnti)
{
  LOG_D(GTPU,
        "[%ld] Start update tunnels for old RNTI %x, new RNTI %x, num_tunnels %d, sgw_S1u_teid %x, eps_bearer_id %x\n",
        instance,
        prior_rnti,
        create_tunnel_req->rnti,
        create_tunnel_req->num_tunnels,
        create_tunnel_req->sgw_S1u_teid[0],
        create_tunnel_req->eps_bearer_id[0]);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));

  if (inst->ue2te_mapping.find(create_tunnel_req->rnti) == inst->ue2te_mapping.end()) {
    LOG_E(GTPU,
          "[%ld] Update not already existing tunnel (new rnti %x, old rnti %x)\n",
          instance,
          create_tunnel_req->rnti,
          prior_rnti);
  }

  auto it = inst->ue2te_mapping.find(prior_rnti);

  if (it != inst->ue2te_mapping.end()) {
    pthread_mutex_unlock(&globGtp.gtp_lock);
    AssertFatal(false,
                "logic bug: update of non-existing tunnel (new ue id %u, old ue id %u)\n",
                create_tunnel_req->rnti,
                prior_rnti);
    /* we don't know if we need 4G or 5G PDCP and can therefore not create a
     * new tunnel */
    return 0;
  }

  inst->ue2te_mapping[create_tunnel_req->rnti] = it->second;
  inst->ue2te_mapping.erase(it);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  return 0;
}

int gtpv1u_create_ngu_tunnel(const instance_t instance,
                             const gtpv1u_gnb_create_tunnel_req_t *const create_tunnel_req,
                             gtpv1u_gnb_create_tunnel_resp_t *const create_tunnel_resp,
                             gtpCallback callBack,
                             gtpCallbackSDAP callBackSDAP)
{
  LOG_D(GTPU,
        "[%ld] Create tunnel for UE ID %lu, outgoing TEID 0x%x\n",
        instance,
        create_tunnel_req->ue_id,
        create_tunnel_req->outgoing_teid);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));

  uint8_t addr[inst->foundAddrLen];
  memcpy(addr, inst->foundAddr, inst->foundAddrLen);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  teid_t teid = newGtpuCreateTunnel(instance,
                                    create_tunnel_req->ue_id,
                                    create_tunnel_req->incoming_rb_id,
                                    create_tunnel_req->pdusession_id,
                                    create_tunnel_req->outgoing_teid,
                                    create_tunnel_req->dst_addr,
                                    callBack,
                                    callBackSDAP);
  /* Fill response */
  create_tunnel_resp->status = 0;
  create_tunnel_resp->ue_id = create_tunnel_req->ue_id;
  create_tunnel_resp->gnb_NGu_teid = teid;
  memcpy(create_tunnel_resp->gnb_addr.buffer, addr, sizeof(addr));
  create_tunnel_resp->gnb_addr.length = sizeof(addr);
  create_tunnel_resp->pdusession_id = create_tunnel_req->pdusession_id;
  return !GTPNOK;
}

int gtpv1u_update_ue_id(const instance_t instanceP, ue_id_t old_ue_id, ue_id_t new_ue_id)
{
  pthread_mutex_lock(&globGtp.gtp_lock);

  auto inst = &globGtp.instances[compatInst(instanceP)];
  auto it = inst->ue2te_mapping.find(old_ue_id);
  if (it == inst->ue2te_mapping.end()) {
    LOG_W(GTPU, "[%ld] Update GTP tunnels for UEid: %lx, but no tunnel exits\n", instanceP, old_ue_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  for (unsigned i = 0; i < it->second.bearers.size(); ++i) {
    teid_t incoming_teid = inst->ue2te_mapping[old_ue_id].bearers[i].teid_incoming;
    if (globGtp.te2ue_mapping[incoming_teid].ue_id == old_ue_id) {
      globGtp.te2ue_mapping[incoming_teid].ue_id = new_ue_id;
    }
  }

  inst->ue2te_mapping[new_ue_id] = it->second;
  inst->ue2te_mapping.erase(it);

  pthread_mutex_unlock(&globGtp.gtp_lock);

  LOG_I(GTPU, "[%ld] Updated tunnels from UEid %lx to UEid %lx\n", instanceP, old_ue_id, new_ue_id);
  return !GTPNOK;
}

int gtpv1u_create_x2u_tunnel(const instance_t instanceP,
                             const gtpv1u_enb_create_x2u_tunnel_req_t *const create_tunnel_req_pP,
                             gtpv1u_enb_create_x2u_tunnel_resp_t *const create_tunnel_resp_pP)
{
  UNUSED(instanceP);
  UNUSED(create_tunnel_req_pP);
  UNUSED(create_tunnel_resp_pP);
  AssertFatal(false, "to be developped\n");
}

int newGtpuDeleteOneTunnel(instance_t instance, ue_id_t ue_id, int rb_id)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  map<uint64_t, teidData_t>::iterator ue_it = inst->ue2te_mapping.find(ue_id);
  if (ue_it == inst->ue2te_mapping.end()) {
    LOG_E(GTPU, "%s() no such UE %ld\n", __func__, ue_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return !GTPNOK;
  }
  map<ue_id_t, gtpv1u_bearer_t>::iterator rb_it = ue_it->second.bearers.find(rb_id);
  if (rb_it == ue_it->second.bearers.end()) {
    LOG_E(GTPU, "%s() UE %ld has no tunnel for bearer %d\n", __func__, ue_id, rb_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return !GTPNOK;
  }
  teid_t teid = rb_it->second.teid_incoming;
  globGtp.te2ue_mapping.erase(teid);
  ue_it->second.bearers.erase(rb_id);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "Deleted tunnel TEID 0x%x for bearer %d of UE %ld, remaining tunnels:\n", teid, rb_id, ue_id);
  for (auto b : ue_it->second.bearers)
    LOG_I(GTPU, "Bearer %ld\n", b.first);
  return !GTPNOK;
}

int newGtpuDeleteAllTunnels(instance_t instance, ue_id_t ue_id)
{
  LOG_D(GTPU, "[%ld] Start delete tunnels for ue id %lu\n", instance, ue_id);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  getUeRetInt(inst, ue_id);

  int nb = 0;

  for (auto j = ptrUe->second.bearers.begin(); j != ptrUe->second.bearers.end(); ++j) {
    globGtp.te2ue_mapping.erase(j->second.teid_incoming);
    nb++;
  }

  inst->ue2te_mapping.erase(ptrUe);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "[%ld] UE ID %ld: Delete all tunnels (%d tunnels)\n", instance, ue_id, nb);
  return !GTPNOK;
}

int gtpv1u_delete_s1u_tunnel(const instance_t instance, const gtpv1u_enb_delete_tunnel_req_t *const req_pP)
{
  LOG_D(GTPU, "[%ld] Start delete tunnels for RNTI %x\n", instance, req_pP->rnti);
  pthread_mutex_lock(&globGtp.gtp_lock);
  auto inst = &globGtp.instances[compatInst(instance)];
  auto ptrRNTI = inst->ue2te_mapping.find(req_pP->rnti);
  if (ptrRNTI == inst->ue2te_mapping.end()) {
    LOG_W(GTPU, "[%ld] Delete Released GTP tunnels for rnti: %x, but no tunnel exits\n", instance, req_pP->rnti);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return -1;
  }

  int nb = 0;

  for (int i = 0; i < req_pP->num_erab; i++) {
    auto ptr2 = ptrRNTI->second.bearers.find(req_pP->eps_bearer_id[i]);
    if (ptr2 == ptrRNTI->second.bearers.end()) {
      LOG_E(GTPU,
            "[%ld] GTP-U instance: delete of not existing tunnel RNTI:RAB: %x/%x\n",
            instance,
            req_pP->rnti,
            req_pP->eps_bearer_id[i]);
    } else {
      globGtp.te2ue_mapping.erase(ptr2->second.teid_incoming);
      nb++;
    }
  }

  if (ptrRNTI->second.bearers.size() == 0)
    // no tunnels on this rnti, erase the ue entry
    inst->ue2te_mapping.erase(ptrRNTI);

  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "[%ld] Deleted released tunnels for RNTI %x (%d tunnels deleted)\n", instance, req_pP->rnti, nb);
  return !GTPNOK;
}

// Legacy delete tunnel finish by deleting all the ue id
int gtpv1u_delete_all_s1u_tunnel(const instance_t instance, const rnti_t rnti)
{
  return newGtpuDeleteAllTunnels(instance, rnti);
}

int gtpv1u_delete_x2u_tunnel(const instance_t instanceP, const gtpv1u_enb_delete_tunnel_req_t *const req_pP)
{
  UNUSED(instanceP);
  UNUSED(req_pP);
  LOG_E(GTPU, "x2 tunnel not implemented\n");
  return 0;
}

static gtpv1u_bearer_t create_bearer(int socket, const struct sockaddr_in *addr, uint32_t teid, uint16_t seq)
{
  gtpv1u_bearer_t bearer = {.sock_fd = socket, .teid_outgoing = teid, .seqNum = seq};
  memcpy(&bearer.ip, addr, sizeof(*addr));
  return bearer;
}

static int Gtpv1uHandleEchoReq(int h, uint8_t *msgBuf, const struct sockaddr_in *addr)
{
  Gtpv1uMsgHeaderT *msgHdr = (Gtpv1uMsgHeaderT *)msgBuf;

  if (msgHdr->version != 1 || msgHdr->PT != 1) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  if (msgHdr->S != 1) {
    LOG_E(GTPU, "[%d] Received a echo request packet with no sequence number \n", h);
    return GTPNOK;
  }

  uint16_t seq = ntohs(*(uint16_t *)(msgHdr + 1));
  LOG_D(GTPU, "[%d] Received a echo request, TEID: 0x%x, seq: %hu\n", h, msgHdr->teid, seq);
  uint8_t recovery[2] = {14, 0};
  gtpv1u_bearer_t bearer = create_bearer(h, addr, ntohl(msgHdr->teid), seq);
  return gtpv1uCreateAndSendMsg(&bearer,
                                GTP_ECHO_RSP,
                                recovery,
                                sizeof recovery,
                                true,
                                false,
                                NULL,
                                0);
}

static int Gtpv1uHandleError(uint8_t *msgBuf, uint32_t msgBufLen)
{
  if (msgBufLen < sizeof(Gtpv1uError))
    LOG_E(GTPU, "Received GTP error indication with truncated size %u (mini size: %lu)\n", msgBufLen,sizeof(Gtpv1uError)+4);
  Gtpv1uError *msg = ( Gtpv1uError *)msgBuf;
  LOG_E(GTPU,
        "Received GTP error indication: \n"
        "   TEID 0x%x (must be 0 from TS 29.281)\n"
        "   TV id for TEID 0x%x (must be 16)\n"
        "   TEID in error 0x%x (should be a TEID we sent)\n"
        "   TV id for GTP addr %u (should be 133)\n"
        "   len for addr of UPF %u (should be IPv4 or IPv6 len)"
        "   (TS 29.281 Sec 7.3.1 Error Handling not implemented)\n",
        ntohl(msg->h.teid),
        msg->teid_data_i,
        ntohl(msg->teid),
        msg->addr_data_i,
        msg->addr_len);
  int rc = GTPNOK;
  return rc;
}

static int Gtpv1uHandleSupportedExt()
{
  LOG_E(GTPU, "Supported extensions to be dev\n");
  int rc = GTPNOK;
  return rc;
}

// When end marker arrives, we notify the client with buffer size = 0
// The client will likely call "delete tunnel"
// nevertheless we don't take the initiative
static int Gtpv1uHandleEndMarker(int h, uint8_t *msgBuf)
{
  Gtpv1uMsgHeaderT *msgHdr = (Gtpv1uMsgHeaderT *)msgBuf;

  if (msgHdr->version != 1 || msgHdr->PT != 1) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  pthread_mutex_lock(&globGtp.gtp_lock);
  // the socket Linux file handler is the instance id
  getInstRetInt(h);

  auto tunnel = globGtp.te2ue_mapping.find(ntohl(msgHdr->teid));

  if (tunnel == globGtp.te2ue_mapping.end()) {
    LOG_E(GTPU, "[%d] Received a incoming packet on unknown TEID (0x%x) Dropping!\n", h, msgHdr->teid);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  // This context is not good for gtp
  // frame, ... has no meaning
  // manyother attributes may come from create tunnel
  protocol_ctxt_t ctxt;
  ctxt.module_id = 0;
  ctxt.enb_flag = 1;
  ctxt.instance = inst->addr.originInstance;
  ctxt.rntiMaybeUEid = tunnel->second.ue_id;
  ctxt.frame = 0;
  ctxt.subframe = 0;
  ctxt.eNB_index = 0;
  ctxt.brOption = 0;
  const srb_flag_t srb_flag = SRB_FLAG_NO;
  const rb_id_t rb_id = tunnel->second.incoming_rb_id;
  const mui_t mui = RLC_MUI_UNDEFINED;
  const confirm_t confirm = RLC_SDU_CONFIRM_NO;
  const pdcp_transmission_mode_t mode = PDCP_TRANSMISSION_MODE_DATA;
  const uint32_t sourceL2Id = 0;
  const uint32_t destinationL2Id = 0;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  if (!tunnel->second.callBack(&ctxt, srb_flag, rb_id, mui, confirm, 0, NULL, mode, &sourceL2Id, &destinationL2Id))
    LOG_E(GTPU, "[%d] down layer refused incoming packet\n", h);

  LOG_D(GTPU, "[%d] Received END marker packet for: TEID:0x%x\n", h, ntohl(msgHdr->teid));
  return !GTPNOK;
}

static int Gtpv1uHandleGpdu(int h, uint8_t *msgBuf, uint32_t msgBufLen, const struct sockaddr_in *addr)
{
  Gtpv1uMsgHeaderT *msgHdr = (Gtpv1uMsgHeaderT *)msgBuf;

  if (msgHdr->version != 1 || msgHdr->PT != 1) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  pthread_mutex_lock(&globGtp.gtp_lock);
  auto tunnel = globGtp.te2ue_mapping.find(ntohl(msgHdr->teid));

  if (tunnel == globGtp.te2ue_mapping.end()) {
    LOG_E(GTPU, "[%d] Received a incoming packet on unknown TEID (0x%x) Dropping!\n", h, ntohl(msgHdr->teid));
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }
  ueidData_t uedata = tunnel->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  /* see TS 29.281 5.1 */
  // Minimum length of GTP-U header if non of the optional fields are present
  unsigned int offset = sizeof(Gtpv1uMsgHeaderT);

  int8_t qfi = -1;
  bool rqi = false;
  uint32_t NR_PDCP_PDU_SN = 0;

  /* if E, S, or PN is set then there are 4 more bytes of header */
  if (msgHdr->E || msgHdr->S || msgHdr->PN)
    offset += 4;

  if (msgHdr->E) {
    int next_extension_header_type = msgBuf[offset - 1];
    int extension_header_length;

    while (next_extension_header_type != NO_MORE_EXT_HDRS) {
      extension_header_length = msgBuf[offset];
      switch (next_extension_header_type) {
        case PDU_SESSION_CONTAINER: {
          if (offset + sizeof(PDUSessionContainerT) > msgBufLen) {
            LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
            return GTPNOK;
          }
          PDUSessionContainerT *pdusession_cntr = (PDUSessionContainerT *)(msgBuf + offset + 1);
          qfi = pdusession_cntr->QFI;
          rqi = pdusession_cntr->Reflective_QoS_activation;
          break;
        }
        case NR_RAN_CONTAINER: {
          if (offset + 1 > msgBufLen) {
            LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
            return GTPNOK;
          }
          uint8_t PDU_type = (msgBuf[offset + 1] >> 4) & 0x0f;
          if (PDU_type == 0) { // DL USER Data Format
            int additional_offset = 6; // Additional offset capturing the first non-mandatory octet (TS 38.425, Figure 5.5.2.1-1)
            if (msgBuf[offset + 1] >> 2 & 0x1) { // DL Discard Blocks flag is present
              LOG_I(GTPU, "DL User Data: DL Discard Blocks handling not enabled\n");
              additional_offset = additional_offset + 9; // For the moment ignore
            }
            if (msgBuf[offset + 1] >> 1 & 0x1) { // DL Flush flag is present
              LOG_I(GTPU, "DL User Data: DL Flush handling not enabled\n");
              additional_offset = additional_offset + 3; // For the moment ignore
            }
            if ((msgBuf[offset + 2] >> 3) & 0x1) { //"Report delivered" enabled (TS 38.425, 5.4)
              /*Store the NR PDCP PDU SN for which a delivery status report shall be generated once the
               *PDU gets forwarded to the lower layers*/
              // NR_PDCP_PDU_SN = msgBuf[offset+6] << 16 | msgBuf[offset+7] << 8 | msgBuf[offset+8];
              NR_PDCP_PDU_SN = msgBuf[offset + additional_offset] << 16 | msgBuf[offset + additional_offset + 1] << 8
                               | msgBuf[offset + additional_offset + 2];
              LOG_D(GTPU, " NR_PDCP_PDU_SN: %u \n", NR_PDCP_PDU_SN);
            }
          } else {
            LOG_W(GTPU, "NR-RAN container type: %d not supported \n", PDU_type);
          }
          break;
        }
        default:
          LOG_W(GTPU, "unhandled extension 0x%2.2x, skipping\n", next_extension_header_type);
          break;
      }

      offset += extension_header_length * EXT_HDR_LNTH_OCTET_UNITS;
      if (offset > msgBufLen) {
        LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
        return GTPNOK;
      }
      next_extension_header_type = msgBuf[offset - 1];
    }
  }

  // This context is not good for gtp
  // frame, ... has no meaning
  // manyother attributes may come from create tunnel
  protocol_ctxt_t ctxt = { .enb_flag = 1, .rntiMaybeUEid = uedata.ue_id, };
  const srb_flag_t srb_flag = SRB_FLAG_NO;
  uint16_t rb_id = uedata.incoming_rb_id;
  const mui_t mui = RLC_MUI_UNDEFINED;
  const confirm_t confirm = RLC_SDU_CONFIRM_NO;
  const sdu_size_t sdu_buffer_size = msgBufLen - offset;
  unsigned char *const sdu_buffer = msgBuf + offset;
  const pdcp_transmission_mode_t mode = PDCP_TRANSMISSION_MODE_DATA;
  const uint32_t sourceL2Id = 0;
  const uint32_t destinationL2Id = 0;

  if (sdu_buffer_size > 0) {
    if (qfi != NO_QFI && uedata.callBackSDAP) {
      if (!uedata.callBackSDAP(&ctxt,
                                       uedata.ue_id,
                                       srb_flag,
                                       mui,
                                       confirm,
                                       sdu_buffer_size,
                                       sdu_buffer,
                                       mode,
                                       &sourceL2Id,
                                       &destinationL2Id,
                                       qfi,
                                       rqi,
                                       uedata.pdusession_id))
        LOG_E(GTPU, "[%d] down layer refused incoming SDAP packet\n", h);
    } else {
      /* Non-SDAP callback path: direct TEID-to-incoming_rb_id delivery via callBack.
       * QFI must be absent on this path */
      AssertFatal(qfi == NO_QFI,
                  "[%d] Non-SDAP callback configured but QFI=%d is present (ue=%lu teid=0x%x)\n",
                  h,
                  qfi,
                  uedata.ue_id,
                  ntohl(msgHdr->teid));
      if (!uedata.callBack(&ctxt, srb_flag, rb_id, mui, confirm, sdu_buffer_size, sdu_buffer, mode, &sourceL2Id, &destinationL2Id))
        LOG_E(GTPU, "[%d] down layer refused incoming packet\n", h);
    }
  }

  /* Delivery status report path uses DRB-based RLC state: keep it on non-SDAP path only. */
  if (!uedata.callBackSDAP && NR_PDCP_PDU_SN > 0 && NR_PDCP_PDU_SN % 5 == 0) {
    LOG_D(GTPU, "Create and send DL DATA Delivery status for the previously received PDU, NR_PDCP_PDU_SN: %u \n", NR_PDCP_PDU_SN);
    int rlc_tx_buffer_space = nr_rlc_get_available_tx_space(ctxt.rntiMaybeUEid, rb_id + 3);
    LOG_D(GTPU, "Available buffer size in RLC for Tx: %d \n", rlc_tx_buffer_space);
    gtpu_extension_header_t ext;
    fillDlDeliveryStatusReport(&ext, rlc_tx_buffer_space, NR_PDCP_PDU_SN);
    uint32_t teid = globGtp.te2ue_mapping[ntohl(msgHdr->teid)].outgoing_teid;
    gtpv1u_bearer_t bearer = create_bearer(h, addr, teid, 0);
    gtpv1uCreateAndSendMsg(&bearer,
                           GTP_GPDU,
                           NULL,
                           0,
                           false,
                           false,
                           &ext,
                           1);
  }

  LOG_D(GTPU, "[%d] Received a %d bytes packet for: TEID:0x%x\n", h, msgBufLen - offset, ntohl(msgHdr->teid));
  return !GTPNOK;
}

static bool gtpv1uReceiveHandleMessage(int h, uint8_t buf[VLEN][BUFSIZE])
{
  struct iovec iovecs[VLEN];
  struct mmsghdr msgs[VLEN];
  struct sockaddr_in addr[VLEN];

  for (size_t i = 0; i < VLEN; ++i) {
    iovecs[i].iov_base = buf[i];
    iovecs[i].iov_len = BUFSIZE;
    msgs[i].msg_hdr.msg_iov = &iovecs[i];
    msgs[i].msg_hdr.msg_iovlen = 1;
    msgs[i].msg_hdr.msg_name = &addr[i];
    msgs[i].msg_hdr.msg_namelen = (socklen_t)sizeof(struct sockaddr_in);
  };

  int ret = recvmmsg(h, msgs, VLEN, MSG_WAITFORONE, NULL);
  if (ret < 0) {
    LOG_E(GTPU, "[%d] Recvfrom failed (%s)\n", h, strerror(errno));
    return false;
  }

  for (int i = 0; i < ret; ++i) {
    int udpDataLen = msgs[i].msg_len;
    uint8_t *udpData = buf[i];
    if (udpDataLen < (int)sizeof(Gtpv1uMsgHeaderT)) {
      LOG_W(GTPU, "[%d] received malformed gtp packet \n", h);
      return true;
    }
    Gtpv1uMsgHeaderT *msg = (Gtpv1uMsgHeaderT *)udpData;
    if ((int)(ntohs(msg->msgLength) + sizeof(Gtpv1uMsgHeaderT)) != udpDataLen) {
      LOG_W(GTPU, "[%d] received malformed gtp packet length\n", h);
      return true;
    }
    LOG_D(GTPU, "[%d] Received GTP data, msg type: %x\n", h, msg->msgType);
    switch (msg->msgType) {
      case GTP_ECHO_RSP:
        break;

      case GTP_ECHO_REQ:
        Gtpv1uHandleEchoReq(h, udpData, &addr[i]);
        break;

      case GTP_ERROR_INDICATION:
        Gtpv1uHandleError(udpData, udpDataLen);
        break;

      case GTP_SUPPORTED_EXTENSION_HEADER_INDICATION:
        Gtpv1uHandleSupportedExt();
        break;

      case GTP_END_MARKER:
        Gtpv1uHandleEndMarker(h, udpData);
        break;

      case GTP_GPDU:
        Gtpv1uHandleGpdu(h, udpData, udpDataLen, &addr[i]);
        break;

      default:
        LOG_E(GTPU, "[%d] Received a GTP packet of unknown type: %d\n", h, msg->msgType);
        break;
    }
  }
  return true;
}

static void* gtpv1uReceiver(void *thr)
{
  gtpThread_t *gt = (gtpThread_t *)thr;
  /* this buffer is 1MB large, ok because at the bottom of the stack */
  uint8_t buf[VLEN][BUFSIZE];
  while (gtpv1uReceiveHandleMessage(gt->h, buf)) {
  }
  LOG_W(GTPU, "exiting thread\n");
  return NULL;
}

static void gtpv1uReceiverCancel(pthread_t t)
{
  int rc;
  rc = pthread_cancel(t);
  DevAssert(rc == 0);
  rc = pthread_join(t, NULL);
  DevAssert(rc == 0);
}

#include <openair2/ENB_APP/enb_paramdef.h>

void *gtpv1uTask(void *args)
{
  UNUSED(args);
  while (1) {
    /* Trying to fetch a message from the message queue.
       If the queue is empty, this function will block till a
       message is sent to the task.
    */
    MessageDef *message_p = NULL;
    itti_receive_msg(TASK_GTPV1_U, &message_p);

    if (message_p != NULL) {
      openAddr_t addr = {{0}};
      const instance_t myInstance = ITTI_MSG_DESTINATION_INSTANCE(message_p);
      const int msgType = ITTI_MSG_ID(message_p);
      LOG_D(GTPU, "GTP-U received %s for instance %ld\n", messages_info[msgType].name, myInstance);
      switch (msgType) {
          // DATA TO BE SENT TO UDP

        case TERMINATE_MESSAGE:
          LOG_W(GTPU, "Exiting GTP instance %ld\n", myInstance);
          itti_exit_task();
          break;

        case TIMER_HAS_EXPIRED:
          LOG_E(GTPU, "Received unexpected timer expired (no need of timers in this version) %s\n", ITTI_MSG_NAME(message_p));
          break;

        case GTPV1U_ENB_END_MARKER_REQ:
          gtpv1uEndTunnel(compatInst(myInstance), &GTPV1U_ENB_END_MARKER_REQ(message_p));
          itti_free(TASK_GTPV1_U, GTPV1U_ENB_END_MARKER_REQ(message_p).buffer);
          break;

        case GTPV1U_ENB_DATA_FORWARDING_REQ:
        case GTPV1U_ENB_DATA_FORWARDING_IND:
        case GTPV1U_ENB_END_MARKER_IND:
          LOG_E(GTPU, "to be developped %s\n", ITTI_MSG_NAME(message_p));
          abort();
          break;

        case GTPV1U_REQ:
          // to be dev: should be removed, to use API
          strcpy(addr.originHost, GTPV1U_REQ(message_p).localAddrStr);
          strcpy(addr.originService, GTPV1U_REQ(message_p).localPortStr);
          strcpy(addr.destinationService, addr.originService);
          AssertFatal((legacyInstanceMapping = gtpv1Init(addr)) != 0, "Instance 0 reserved for legacy\n");
          break;

        default:
          LOG_E(GTPU, "Received unexpected message %s\n", ITTI_MSG_NAME(message_p));
          abort();
          break;
      }

      AssertFatal(EXIT_SUCCESS == itti_free(TASK_GTPV1_U, message_p), "Failed to free memory!\n");
    }
  }

  return NULL;
}

#ifdef __cplusplus
}
#endif
