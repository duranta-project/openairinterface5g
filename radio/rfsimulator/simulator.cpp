/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Open issues and limitations
 * The read and write should be called in the same thread, that is not new USRP UHD design
 * When the opposite side switch from passive reading to active R+Write, the synchro is not fully deterministic
 */

#include "PHY/TOOLS/tools_defs.h"
#include "PHY/defs_common.h"
#include "utils.h"
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/epoll.h>
#include <netdb.h>

#include <common/utils/assertions.h>
#include <common/utils/LOG/log.h>
#include <common/utils/telnetsrv/telnetsrv.h>
#include <common/config/config_userapi.h>
#include "common_lib.h"
#include "common/utils/threadPool/pthread_utils.h"
extern "C" {
#include <common/utils/load_module_shlib.h>
#include <openair1/SIMULATION/TOOLS/sim.h>
#include "rfsimulator.h"
extern int get_currentchannels_type(const char *buf,
                                    int debug,
                                    webdatadef_t *tdata,
                                    telnet_printfunc_t prnt); // in random_channel.c
}
#include <queue>
#include <mutex>
#include <vector>
#include <sstream>
#include <algorithm>
#include <numeric>

#define PORT 4043 // default TCP port for this simulator
#define sampleToByte(a, b) ((a) * (b) * sizeof(sample_t))
#define byteToSample(a, b) ((a) / (sizeof(sample_t) * (b)))

#define GENERATE_CHANNEL 10 // each frame (or slot?) in DL
#define MAX_FD_RFSIMU 250
#define SEND_BUFF_SIZE 100000000 // Socket buffer size
#define MAX_BEAMS 64

// Simulator role
typedef enum { SIMU_ROLE_SERVER = 1, SIMU_ROLE_CLIENT } simuRole;

//

#define RFSIMU_SECTION "rfsimulator"
#define RFSIMU_SERVER_ADDR "serveraddr"
#define RFSIMU_SERVER_PORT "serverport"
#define RFSIMU_OPTIONS_PARAMNAME "options"
#define RFSIMU_IQFILE "IQfile"
#define RFSIMU_MODELNAME "modelname"
#define RFSIMU_PLOSS "ploss"
#define RFSIMU_FORGETFACT "forgetfact"
#define RFSIMU_OFFSET "offset"
#define RFSIMU_PROP_DELAY "prop_delay"
#define RFSIMU_WAIT_TIMEOUT "wait_timeout"
#define RFSIMU_ENABLE_BEAMS "enable_beams"
#define RFSIMU_NUM_CONCURRENT_BEAMS "num_concurrent_beams"
#define RFSIMU_BEAM_MAP "beam_map"
#define RFSIMU_BEAM_GAINS "beam_gains"
#define RFSIMU_BEAM_IDS "beam_ids"

#define RFSIM_CONFIG_HELP_OPTIONS                                                                  \
  " list of comma separated options to enable rf simulator functionalities. Available options: \n" \
  "        chanmod:   enable channel modelisation\n"                                               \
  "        saviq:     enable saving written iqs to a file\n"

#define simOpt PARAMFLAG_NOFREE | PARAMFLAG_CMDLINE_NOPREFIXENABLED
#define simBool PARAMFLAG_BOOL | PARAMFLAG_NOFREE | PARAMFLAG_CMDLINE_NOPREFIXENABLED
// clang-format off
/*----------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            configuration parameters for the rfsimulator device */
/* optname                              helpstr                                 paramflags  XXXptr                            defXXXval */
/*----------------------------------------------------------------------------------------------------------------------------------------------------*/
#define RFSIMULATOR_PARAMS_DESC {					\
  STRINGPARAM(RFSIMU_SERVER_ADDR,       "<ip address to connect to>\n",             simOpt, NULL,                             "127.0.0.1"),           \
  UINT16PARAM(RFSIMU_SERVER_PORT,       "<port to connect to>\n",                   simOpt, NULL,                             PORT),                  \
  STRLISTPARAM(RFSIMU_OPTIONS_PARAMNAME, RFSIM_CONFIG_HELP_OPTIONS,                 simOpt, NULL,                             NULL),                  \
  STRINGPARAM(RFSIMU_IQFILE,            "<file path to use when saving IQs>\n",     simOpt, NULL,                             "/tmp/rfsimulator.iqs"),\
  STRINGPARAM(RFSIMU_MODELNAME,         "<channel model name>\n",                   simOpt, NULL,                             "AWGN"),                \
  DOUBLEPARAM(RFSIMU_PLOSS,             "<channel path loss in dB>\n",              simOpt, NULL,                             0),                     \
  DOUBLEPARAM(RFSIMU_FORGETFACT,        "<channel forget factor ((0 to 1)>\n",      simOpt, NULL,                             0),                     \
  UINT64PARAM(RFSIMU_OFFSET,            "<channel offset in samps>\n",              simOpt, NULL,                             0L),                    \
  DOUBLEPARAM(RFSIMU_PROP_DELAY,        "<propagation delay in ms>\n",              simOpt, NULL,                             0.0),                   \
  INTPARAM(RFSIMU_WAIT_TIMEOUT,         "<wait timeout if no UE connected>\n",      simOpt, NULL,                             1),                     \
  BOOLPARAM(RFSIMU_ENABLE_BEAMS,        "<enable simplified beam simulation>\n",    simBool,NULL,                             0),                     \
  INTPARAM(RFSIMU_NUM_CONCURRENT_BEAMS, "<number of concurrent beams supported>\n", simOpt, NULL,                             1),                     \
  UINT64PARAM(RFSIMU_BEAM_MAP,          "<initial beam map>\n",                     simOpt, NULL,                             1),                     \
  STRINGPARAM(RFSIMU_BEAM_IDS,          "<initial beam ids>\n",                     simOpt, NULL,                             NULL),                  \
  STRINGPARAM(RFSIMU_BEAM_GAINS,        "<beam gain matrix in toeplitz form>\n",    simOpt, NULL,                             NULL),                  \
};
// clang-format on
static void getset_currentchannels_type(char *buf, int debug, webdatadef_t *tdata, telnet_printfunc_t prnt);
static int rfsimu_setchanmod_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_setdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_getdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_vtime_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_set_beam(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
static int rfsimu_set_beamids(char *buff, int debug, telnet_printfunc_t prnt, void *arg);
// clang-format off
static telnetshell_cmddef_t rfsimu_cmdarray[] = {
    {"show models", "", (cmdfunc_t)rfsimu_setchanmod_cmd, {(webfunc_t)getset_currentchannels_type}, TELNETSRV_CMDFLAG_WEBSRVONLY | TELNETSRV_CMDFLAG_GETWEBTBLDATA, NULL},
    {"setmodel", "<model name> <model type>", (cmdfunc_t)rfsimu_setchanmod_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_TELNETONLY, NULL},
    {"setdistance", "<model name> <distance>", (cmdfunc_t)rfsimu_setdistance_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_NEEDPARAM },
    {"getdistance", "<model name>", (cmdfunc_t)rfsimu_getdistance_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ},
    {"vtime", "", (cmdfunc_t)rfsimu_vtime_cmd, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ | TELNETSRV_CMDFLAG_AUTOUPDATE},
    {"setbeam", "beam_map", (cmdfunc_t)rfsimu_set_beam, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ},
    {"setbeamids", "beam_id1,beam_id2,...", (cmdfunc_t)rfsimu_set_beamids, {NULL}, TELNETSRV_CMDFLAG_PUSHINTPOOLQ},
    {"", "", NULL},
};
// clang-format on
static telnetshell_cmddef_t *setmodel_cmddef = &(rfsimu_cmdarray[1]);

static telnetshell_vardef_t rfsimu_vardef[] = {{"", 0, 0, NULL}};
typedef c16_t sample_t; // 2*16 bits complex number

typedef struct beam_switch_command_t {
  std::vector<int> beams;
  openair0_timestamp_t timestamp;
} beam_switch_command_t;

typedef struct {
  std::vector<int> beams;
  std::queue<beam_switch_command_t> cmd_queue;
  std::mutex mutex;
} beam_state_t;

typedef struct {
  samplesBlockHeader_t header;
  char payload[1];
} rfsim_packet_t;

typedef struct buffer_s {
  int conn_sock;
  openair0_timestamp_t lastReceivedTS;
  bool headerMode;
  bool trashingPacket;
  samplesBlockHeader_t th;
  uint nbAnt;
  char *transferPtr;
  uint64_t remainToTransfer;
  channel_desc_t *channel_model;
  rfsim_packet_t *packet_ptr;
  size_t payload_sz;
  size_t remainToTransferBeam;
  std::queue<rfsim_packet_t *> received_packets;
} buffer_t;

typedef struct {
  int enable_beams;
  int num_concurrent_beams;
  std::vector<std::vector<float>> beam_gains;
  beam_state_t tx;
  beam_state_t rx;
} rfsim_beam_ctrl_t;

typedef struct {
  int listen_sock, epollfd;
  pthread_mutex_t Sockmutex;
  int ru_id;
  unsigned int nb_cnx;
  openair0_timestamp_t nextRxTstamp;
  openair0_timestamp_t lastWroteTS;
  simuRole role;
  char *ip;
  uint16_t port;
  int saveIQfile;
  buffer_t buf[MAX_FD_RFSIMU];
  int next_buf;
  int rx_num_channels;
  int tx_num_channels;
  double sample_rate;
  double rx_freq;
  double tx_bw;
  int channelmod;
  double chan_pathloss;
  double chan_forgetfact;
  uint64_t chan_offset;
  void *telnetcmd_qid;
  poll_telnetcmdq_func_t poll_telnetcmdq;
  int wait_timeout;
  double prop_delay_ms;
  rfsim_beam_ctrl_t *beam_ctrl;
} rfsimulator_state_t;

/**
 * @brief Get the current beam map for a given timestamp and number of samples.
 *
 * This function retrieves the beam map from the beam state, considering any queued beam switch commands.
 *
 * @param beam_state Pointer to the beam_state_t structure containing the current beam map and command queue.
 * @param timestamp The timestamp for which the beam map is requested.
 * @param nsamps The number of samples to process.
 * @param nsamps_out output pointer to receive the number of samples until the next beam switch.
 * @return The beam map (uint64_t) valid for the given timestamp.
 */
static std::vector<int> get_beams(beam_state_t *beam_state, openair0_timestamp_t timestamp, uint32_t nsamps, uint32_t *nsamps_out)
{
  std::lock_guard<std::mutex> lock(beam_state->mutex);
  std::vector<int> current_beams = beam_state->beams;
  uint32_t samples_to_next_switch = nsamps;

  // Find the latest beam_switch_command_t with timestamp <= requested timestamp
  if (!beam_state->cmd_queue.empty()) {
    // Copy queue to avoid modifying original
    std::queue<beam_switch_command_t> queue_copy = beam_state->cmd_queue;
    while (!queue_copy.empty()) {
      const beam_switch_command_t &cmd = queue_copy.front();
      if (cmd.timestamp <= timestamp) {
        current_beams = cmd.beams;
        queue_copy.pop();
      } else {
        samples_to_next_switch = (cmd.timestamp > timestamp) ? (cmd.timestamp - timestamp) : nsamps;
        break;
      }
    }
  }

  // Clamp samples_to_next_switch to nsamps
  if (samples_to_next_switch > nsamps)
    samples_to_next_switch = nsamps;

  *nsamps_out = samples_to_next_switch;

  return current_beams;
}


static std::vector<int> beam_map_to_beams(uint64_t beam_map)
{
  int num_beams = __builtin_popcountll(beam_map);
  AssertFatal(num_beams > 0, "Needs at least one beam\n");
  std::vector<int> beam_ids;
  for (int i = 0; i < MAX_BEAMS; i++) {
    if (beam_map & (1ULL << i)) {
      beam_ids.push_back(i);
    }
  }
  return beam_ids;
}

static uint64_t beams_to_beam_map(const std::vector<int> &beam_ids)
{
  uint64_t beam_map = 0;
  for (size_t i = 0; i < beam_ids.size(); i++) {
    beam_map |= (1ULL << beam_ids[i]);
  }
  return beam_map;
}


/**
 * @brief Clears outdated beam switch commands from the queue and updates the current beam map.
 *
 * This function processes the beam switch command queue in the given beam_state_t.
 * For each command with a timestamp less than or equal to the provided timestamp,
 * it updates the current beam_map and removes the command from the queue.
 * The queue is left unchanged for commands with timestamps greater than the provided timestamp.
 *
 * @param beam_state Pointer to the beam_state_t structure containing the command queue and current beam map.
 * @param timestamp The timestamp up to which commands should be processed and removed.
 */
static void clear_beam_queue(beam_state_t *beam_state, openair0_timestamp_t timestamp)
{
  std::lock_guard<std::mutex> lock(beam_state->mutex);
  while (!beam_state->cmd_queue.empty()) {
    if (beam_state->cmd_queue.front().timestamp <= timestamp) {
      beam_state->beams = beam_state->cmd_queue.front().beams;
      beam_state->cmd_queue.pop();
    } else {
      break;
    }
  }
}

/**
 * @brief Clears old packets from the received_packets queue based on a threshold timestamp.
 *
 * This function iterates through the received_packets queue and removes packets whose
 * timestamp plus size is less than or equal to the provided threshold_timestamp.
 * It frees the memory allocated for each removed packet.
 *
 * @param received_packets Reference to the queue of rfsim_packet_t pointers representing received packets.
 * @param threshold_timestamp The timestamp threshold used to determine which packets to remove.
 */
static void clear_old_packets(std::queue<rfsim_packet_t *> &received_packets, uint64_t threshold_timestamp)
{
  while (!received_packets.empty()) {
    rfsim_packet_t *pkt = received_packets.front();
    if (pkt->header.timestamp + pkt->header.size <= threshold_timestamp) {
      free(pkt);
      received_packets.pop();
    } else {
      break;
    }
  }
}

static bool flushInput(rfsimulator_state_t *t, int timeout, bool first_time);

static buffer_t *allocCirBuf(rfsimulator_state_t *bridge, int sock)
{
  uint64_t buff_index = bridge->next_buf++ % MAX_FD_RFSIMU;
  buffer_t *ptr = &bridge->buf[buff_index];
  bridge->nb_cnx++;
  ptr->conn_sock = sock;
  ptr->lastReceivedTS = 0;
  ptr->headerMode = true;
  ptr->trashingPacket = true;
  ptr->transferPtr = (char *)&ptr->th;
  ptr->remainToTransfer = sizeof(samplesBlockHeader_t);
  ptr->received_packets = std::queue<rfsim_packet_t *>();
  int sendbuff = SEND_BUFF_SIZE;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)) != 0) {
    LOG_E(HW, "setsockopt(SO_SNDBUF) failed\n");
    return NULL;
  }
  struct epoll_event ev = {0};
  ev.events = EPOLLIN | EPOLLRDHUP;
  ev.data.ptr = ptr;
  if (epoll_ctl(bridge->epollfd, EPOLL_CTL_ADD, sock, &ev) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_ADD) failed\n");
    return NULL;
  }

  if (bridge->channelmod > 0) {
    // create channel simulation model for this mode reception
    char modelname[30];
    snprintf(modelname,
             sizeofArray(modelname),
             "rfsimu_channel_%s%d",
             (bridge->role == SIMU_ROLE_SERVER) ? "ue" : "enB",
             (bridge->role == SIMU_ROLE_SERVER) ? bridge->nb_cnx - 1 : bridge->ru_id);
    ptr->channel_model = find_channel_desc_fromname(modelname); // path_loss in dB
    if (!ptr->channel_model) {
      // Use legacy method to find channel model - this will use the same channel model for all clients
      const char *legacy_model_name = (bridge->role == SIMU_ROLE_SERVER) ? "rfsimu_channel_ue0" : "rfsimu_channel_enB0";
      ptr->channel_model = find_channel_desc_fromname(legacy_model_name);
      if (!ptr->channel_model) {
        LOG_E(HW, "Channel model %s/%s not found, check config file\n", modelname, legacy_model_name);
        return NULL;
      }
    }

    set_channeldesc_owner(ptr->channel_model, RFSIMU_MODULEID);
    set_channeldesc_direction(ptr->channel_model, bridge->role == SIMU_ROLE_SERVER);
    random_channel(ptr->channel_model, false);
    LOG_I(HW, "Random channel %s in rfsimulator activated\n", modelname);
  }
  return ptr;
}

static void removeCirBuf(rfsimulator_state_t *bridge, buffer_t *buf)
{
  if (epoll_ctl(bridge->epollfd, EPOLL_CTL_DEL, buf->conn_sock, NULL) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_DEL) failed\n");
  }
  close(buf->conn_sock);
  // Fixme: no free_channel_desc_scm(bridge->buf[sock].channel_model) implemented
  // a lot of mem leaks
  // free(bridge->buf[sock].channel_model);
  clear_old_packets(buf->received_packets, INT64_MAX);
  *buf = buffer_t{};
  buf->conn_sock = -1;
  bridge->nb_cnx--;
}

static void socketError(rfsimulator_state_t *bridge, buffer_t *buf)
{
  if (buf->conn_sock != -1) {
    LOG_W(HW, "Lost socket\n");
    removeCirBuf(bridge, buf);

    if (bridge->role == SIMU_ROLE_CLIENT)
      exit(1);
  }
}

enum blocking_t { notBlocking, blocking };

static int setblocking(int sock, enum blocking_t active)
{
  int opts = fcntl(sock, F_GETFL);
  if (opts < 0) {
    LOG_E(HW, "fcntl(F_GETFL) failed, errno(%d)\n", errno);
    return -1;
  }

  if (active == blocking)
    opts = opts & ~O_NONBLOCK;
  else
    opts = opts | O_NONBLOCK;

  opts = fcntl(sock, F_SETFL, opts);
  if (opts < 0) {
    LOG_E(HW, "fcntl(F_SETFL) failed, errno(%d)\n", errno);
    return -1;
  }
  return 0;
}

static void fullwrite(int fd, void *_buf, ssize_t count, rfsimulator_state_t *t)
{
  if (t->saveIQfile != -1) {
    if (write(t->saveIQfile, _buf, count) != count)
      LOG_E(HW, "write() in save iq file failed (%d)\n", errno);
  }

  char *buf = static_cast<char *>(_buf);
  ssize_t l;

  while (count) {
    l = write(fd, buf, count);

    if (l == 0) {
      LOG_E(HW, "write() failed, returned 0\n");
      return;
    }

    if (l < 0) {
      if (errno == EINTR)
        continue;

      if (errno == EAGAIN) {
        LOG_D(HW, "write() failed, errno(%d)\n", errno);
        usleep(250);
        continue;
      } else {
        LOG_E(HW, "write() failed, errno(%d)\n", errno);
        return;
      }
    }

    count -= l;
    buf += l;
  }
}

static float get_rx_gain_db(rfsimulator_state_t *rfsimulator, uint rx_beam, uint tx_beam)
{
  if (!rfsimulator->beam_ctrl->enable_beams) {
    return 0;
  }
  AssertFatal(rx_beam < rfsimulator->beam_ctrl->beam_gains.size() && tx_beam < rfsimulator->beam_ctrl->beam_gains[rx_beam].size(),
              "Beam gain for this combination was not provided rx_beam %d tx_beam %d\n",
              rx_beam,
              tx_beam);
  return rfsimulator->beam_ctrl->beam_gains[rx_beam][tx_beam];
}

static int rfsimulator_set_beams(openair0_device_t *device, uint64_t beam_map, openair0_timestamp_t timestamp)
{
  rfsimulator_state_t *s = static_cast<rfsimulator_state_t *>(device->priv);
  rfsim_beam_ctrl_t *beam_ctrl = s->beam_ctrl;
  std::lock_guard<std::mutex> lock_tx(beam_ctrl->tx.mutex);
  std::lock_guard<std::mutex> lock_rx(beam_ctrl->rx.mutex);
  beam_switch_command_t command = {.beams = beam_map_to_beams(beam_map), .timestamp = timestamp};
  beam_ctrl->rx.cmd_queue.emplace(command);
  beam_ctrl->tx.cmd_queue.emplace(command);
  return 0;
}

static int rfsimulator_set_beams_vector(openair0_device_t *device, int *beams, int num_beams, openair0_timestamp_t timestamp)
{
  rfsimulator_state_t *s = static_cast<rfsimulator_state_t *>(device->priv);
  rfsim_beam_ctrl_t *beam_ctrl = s->beam_ctrl;
  std::lock_guard<std::mutex> lock_tx(beam_ctrl->tx.mutex);
  std::lock_guard<std::mutex> lock_rx(beam_ctrl->rx.mutex);
  beam_switch_command_t command = {.beams = std::vector<int>(beams, beams + num_beams), .timestamp = timestamp};
  beam_ctrl->rx.cmd_queue.emplace(command);
  beam_ctrl->tx.cmd_queue.emplace(command);
  return 0;
}

static void process_gains(char *str, rfsim_beam_ctrl_t *beam_ctrl)
{
  int num_gains = 0;
  float gain_array[MAX_BEAMS];
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, ',') && num_gains < MAX_BEAMS) {
    gain_array[num_gains++] = std::stof(token);
  }

  if (num_gains != 0) {
    for (int i = 0; i < num_gains; i++) {
      std::vector<float> beam_gains;
      for (int j = 0; j < num_gains; j++) {
        int diag = abs(i - j);
        beam_gains.push_back(gain_array[diag]);
      }
      beam_ctrl->beam_gains.push_back(beam_gains);
    }
  }
}

static void rfsimulator_readconfig(rfsimulator_state_t *rfsimulator)
{
  configmodule_interface_t *cfg = config_get_if();
  paramdef_t *rfsimuParam;
  paramdef_t rfsimuParams[] = RFSIMULATOR_PARAMS_DESC;
  paramlist_def_t rfsimuParamList = {RFSIMU_SECTION, NULL, 0};
  int ret = config_getlist(cfg, &rfsimuParamList, rfsimuParams, sizeofArray(rfsimuParams), NULL);
  if (ret < 0 || rfsimuParamList.numelt <= 0) {
    ret = config_get(cfg, rfsimuParams, sizeofArray(rfsimuParams), RFSIMU_SECTION);
    AssertFatal(ret >= 0, "configuration couldn't be performed\n");
    LOG_W(HW, "Warning: rfsimulator parameters should be provided as array elements!\n");
    rfsimuParam = rfsimuParams;
  } else {
    int ru_id = rfsimulator->ru_id;
    AssertFatal(rfsimuParamList.numelt > ru_id,
                "no rfsimulator parameters (numelt %d) specified for card %d\n",
                rfsimuParamList.numelt,
                ru_id);
    rfsimuParam = rfsimuParamList.paramarray[ru_id];
  }

  rfsimulator->ip = strdup(*(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_SERVER_ADDR)->strptr));
  rfsimulator->port = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_SERVER_PORT)->u16ptr);
  char *saveF = strdup(*(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_IQFILE)->strptr));
//char *modelname = strdup(*(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_MODELNAME)->strptr));
  rfsimulator->chan_pathloss = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_PLOSS)->dblptr);
  rfsimulator->chan_forgetfact = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_FORGETFACT)->dblptr);
  rfsimulator->chan_offset = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_OFFSET)->u64ptr);
  rfsimulator->prop_delay_ms = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_PROP_DELAY)->dblptr);
  rfsimulator->wait_timeout = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_WAIT_TIMEOUT)->iptr);

  rfsim_beam_ctrl_t *beam_ctrl = rfsimulator->beam_ctrl;
  beam_ctrl->enable_beams = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_ENABLE_BEAMS)->iptr);
  beam_ctrl->num_concurrent_beams = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_NUM_CONCURRENT_BEAMS)->iptr);
  uint64_t beam_map = *(gpd(rfsimuParam, sizeofArray(rfsimuParams), RFSIMU_BEAM_MAP)->u64ptr);

  rfsimulator->saveIQfile = -1;

  int p = config_paramidx_fromname(rfsimuParams, sizeofArray(rfsimuParams), RFSIMU_OPTIONS_PARAMNAME);
  for (int i = 0; i < rfsimuParam[p].numelt; i++) {
    if (strcmp(rfsimuParam[p].strlistptr[i], "saviq") == 0) {
      rfsimulator->saveIQfile = open(saveF, O_APPEND | O_CREAT | O_TRUNC | O_WRONLY, 0666);

      if (rfsimulator->saveIQfile != -1)
        LOG_I(HW, "Will save written IQ samples in %s\n", saveF);
      else {
        LOG_E(HW, "open(%s) failed for IQ saving, errno(%d)\n", saveF, errno);
        exit(-1);
      }

      break;
    } else if (strcmp(rfsimuParam[p].strlistptr[i], "chanmod") == 0) {
      init_channelmod();
      load_channellist(rfsimulator->tx_num_channels,
                       rfsimulator->rx_num_channels,
                       rfsimulator->sample_rate,
                       rfsimulator->rx_freq,
                       rfsimulator->tx_bw);
      rfsimulator->channelmod = true;
    } else {
      fprintf(stderr, "unknown rfsimulator option: %s\n", rfsimuParam[p].strlistptr[i]);
      exit(-1);
    }
  }

  int beam_gains_param_index = config_paramidx_fromname(rfsimuParams, sizeofArray(rfsimuParams), RFSIMU_BEAM_GAINS);
  if (rfsimuParam[beam_gains_param_index].strptr) {
    process_gains(*rfsimuParam[beam_gains_param_index].strptr, beam_ctrl);
  }

  std::vector<int> initial_beams = beam_map_to_beams(beam_map);
  beam_ctrl->rx.beams = initial_beams;
  beam_ctrl->tx.beams = initial_beams;

  int beam_ids_param_index = config_paramidx_fromname(rfsimuParams, sizeofArray(rfsimuParams), RFSIMU_BEAM_IDS);
  if (rfsimuParam[beam_ids_param_index].strptr) {
    std::vector<int> beam_ids;
    std::stringstream ss(*rfsimuParam[beam_ids_param_index].strptr);
    std::string token;
    while (std::getline(ss, token, ',')) {
      beam_ids.push_back(std::stoi(token));
    }
    beam_ctrl->rx.beams = beam_ids;
    beam_ctrl->tx.beams = beam_ids;
  }

  if (strncasecmp(rfsimulator->ip, "enb", 3) == 0 || strncasecmp(rfsimulator->ip, "server", 3) == 0)
    rfsimulator->role = SIMU_ROLE_SERVER;
  else
    rfsimulator->role = SIMU_ROLE_CLIENT;
}

static int rfsimu_set_beam(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  UNUSED(debug);
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  rfsim_beam_ctrl_t *beam_ctrl = t->beam_ctrl;
  AssertFatal(beam_ctrl->enable_beams, "Beam simualtion is disabled, cannot set beams\n");
  uint64_t beam_map = strtoull(buff, NULL, 0);
  std::lock_guard<std::mutex> lock_tx(beam_ctrl->tx.mutex);
  std::lock_guard<std::mutex> lock_rx(beam_ctrl->rx.mutex);
  beam_ctrl->rx.beams = beam_map_to_beams(beam_map);
  beam_ctrl->tx.beams = beam_map_to_beams(beam_map);
  prnt("Beam map set to 0x%lx\n", beam_map);
  return CMDSTATUS_FOUND;
}

static int rfsimu_set_beamids(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  UNUSED(debug);
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  rfsim_beam_ctrl_t *beam_ctrl = t->beam_ctrl;
  AssertFatal(beam_ctrl->enable_beams, "Beam simualtion is disabled, cannot set beams\n");
  std::vector<int> beam_ids;
  std::stringstream ss(buff);
  std::string token;
  while (std::getline(ss, token, ',')) {
    beam_ids.push_back(std::stoi(token));
  }
  std::lock_guard<std::mutex> lock_tx(beam_ctrl->tx.mutex);
  std::lock_guard<std::mutex> lock_rx(beam_ctrl->rx.mutex);
  beam_ctrl->rx.beams = beam_ids;
  beam_ctrl->tx.beams = beam_ids;
  prnt("Beam ids set\n");
  return CMDSTATUS_FOUND;
}

static int rfsimu_setchanmod_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  char *modelname = NULL;
  char *modeltype = NULL;
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  if (t->channelmod == false) {
    prnt("%s: ERROR channel modelisation disabled...\n", __func__);
    return 0;
  }
  if (buff == NULL) {
    prnt("%s: ERROR wrong rfsimu setchannelmod command...\n", __func__);
    return 0;
  }
  if (debug)
    prnt("%s: rfsimu_setchanmod_cmd buffer \"%s\"\n", __func__, buff);
  int s = sscanf(buff, "%m[^ ] %ms\n", &modelname, &modeltype);

  if (s == 2) {
    int channelmod = modelid_fromstrtype(modeltype);

    if (channelmod < 0)
      prnt("%s: ERROR: model type %s unknown\n", __func__, modeltype);
    else {
      rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
      int found = 0;
      for (int i = 0; i < MAX_FD_RFSIMU; i++) {
        buffer_t *b = &t->buf[i];
        if (b->channel_model == NULL)
          continue;
        if (b->channel_model->model_name == NULL)
          continue;
        if (b->conn_sock >= 0 && (strcmp(b->channel_model->model_name, modelname) == 0)) {
          channel_desc_t *newmodel = new_channel_desc_scm(t->tx_num_channels,
                                                          t->rx_num_channels,
                                                          static_cast<SCM_t>(channelmod),
                                                          t->sample_rate,
                                                          t->rx_freq,
                                                          t->tx_bw,
                                                          30e-9, // TDL delay-spread parameter
                                                          0.0,
                                                          CORR_LEVEL_LOW,
                                                          t->chan_forgetfact, // forgetting_factor
                                                          t->chan_offset, // propagation delay in samples
                                                          t->chan_pathloss,
                                                          0); // noise_power
          set_channeldesc_owner(newmodel, RFSIMU_MODULEID);
          set_channeldesc_direction(newmodel, t->role == SIMU_ROLE_SERVER);
          set_channeldesc_name(newmodel, modelname);
          random_channel(newmodel, false);
          channel_desc_t *oldmodel = b->channel_model;
          b->channel_model = newmodel;
          free_channel_desc_scm(oldmodel);
          prnt("%s: New model type %s applied to channel %s connected to sock %d\n", __func__, modeltype, modelname, i);
          found = 1;
          break;
        }
      } /* for */
      if (found == 0)
        prnt("%s: Channel %s not found or not currently used\n", __func__, modelname);
    }
  } else {
    prnt("%s: ERROR: 2 parameters required: model name and model type (%i found)\n", __func__, s);
  }

  free(modelname);
  free(modeltype);
  return CMDSTATUS_FOUND;
}

static void getset_currentchannels_type(char *buf, int debug, webdatadef_t *tdata, telnet_printfunc_t prnt)
{
  if (strncmp(buf, "set", 3) == 0) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "setmodel %s %s", tdata->lines[0].val[1], tdata->lines[0].val[3]);
    push_telnetcmd_func_t push_telnetcmd = (push_telnetcmd_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_PUSHCMD_FNAME);
    push_telnetcmd(setmodel_cmddef, cmd, prnt);
  } else {
    get_currentchannels_type("modify type", debug, tdata, prnt);
  }
}

static int rfsimu_setdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  if (debug)
    prnt("%s() buffer \"%s\"\n", __func__, buff);

  char *modelname;
  int distance;
  int s = sscanf(buff, "%m[^ ] %d\n", &modelname, &distance);
  if (s != 2) {
    prnt("%s: require exact two parameters\n", __func__);
    return CMDSTATUS_VARNOTFOUND;
  }

  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const double sample_rate = t->sample_rate;
  const double c = (double) SPEED_OF_LIGHT;

  const uint64_t new_offset = (double)distance * sample_rate / c;
  const double new_distance = (double)new_offset * c / sample_rate;
  const double new_delay_ms = new_offset * 1000.0 / sample_rate;

  prnt("\n%s: new_offset %lu, new (exact) distance %.3f m, new delay %f ms\n", __func__, new_offset, new_distance, new_delay_ms);
  t->prop_delay_ms = new_delay_ms;
  t->chan_offset = new_offset;

  /* Set distance in rfsim and channel model, update channel and ringbuffer */
  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];
    if (b->conn_sock <= 0 || b->channel_model == NULL || b->channel_model->model_name == NULL
        || strcmp(b->channel_model->model_name, modelname) != 0) {
      if (b->channel_model != NULL && b->channel_model->model_name != NULL)
        prnt("  %s: model %s unmodified\n", __func__, b->channel_model->model_name);
      continue;
    }

    channel_desc_t *cd = b->channel_model;
    cd->channel_offset = new_offset;
  }

  free(modelname);

  return CMDSTATUS_FOUND;
}

static int rfsimu_getdistance_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  if (debug)
    prnt("%s() buffer \"%s\"\n", __func__, (buff != NULL) ? buff : "NULL");

  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const double sample_rate = t->sample_rate;
  const double c = (double) SPEED_OF_LIGHT;

  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];
    if (b->conn_sock <= 0 || b->channel_model == NULL || b->channel_model->model_name == NULL)
      continue;

    channel_desc_t *cd = b->channel_model;
    const uint64_t offset = cd->channel_offset;
    const double distance = (double)offset * c / sample_rate;
    prnt("%s: %s offset %lu distance %.3f m\n", __func__, cd->model_name, offset, distance);
  }
  prnt("%s: <default> offset %lu delay %f ms\n", __func__, t->chan_offset, t->prop_delay_ms);

  return CMDSTATUS_FOUND;
}

static int rfsimu_vtime_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  UNUSED(debug);
  UNUSED(buff);
  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  const openair0_timestamp_t ts = t->nextRxTstamp;
  const double sample_rate = t->sample_rate;
  prnt("%s: vtime measurement: TS %llu sample_rate %.3f\n", __func__, ts, sample_rate);
  return CMDSTATUS_FOUND;
}

static int startServer(openair0_device_t *device)
{
  int sock = -1;
  struct addrinfo *results = NULL;
  struct addrinfo *rp = NULL;

  rfsimulator_state_t *t = (rfsimulator_state_t *)device->priv;
  t->role = SIMU_ROLE_SERVER;

  char port[6];
  snprintf(port, sizeof(port), "%d", t->port);

  struct addrinfo hints = {
      .ai_flags = AI_PASSIVE,
      .ai_family = AF_INET6,
      .ai_socktype = SOCK_STREAM,
  };

  int s = getaddrinfo(NULL, port, &hints, &results);
  if (s != 0) {
    LOG_E(HW, "getaddrinfo: %s\n", gai_strerror(s));
    freeaddrinfo(results);
    return -1;
  }

  int enable = 1;
  int disable = 0;
  for (rp = results; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(int)) != 0) {
      continue;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
      continue;
    }

    if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }

    close(sock);
    sock = -1;
  }

  freeaddrinfo(results);

  if (sock <= 0) {
    LOG_E(HW, "could not open a socket\n");
    return -1;
  }
  t->listen_sock = sock;

  if (listen(t->listen_sock, 5) != 0) {
    LOG_E(HW, "listen() failed, errno(%d)\n", errno);
    return -1;
  }
  struct epoll_event ev = {0};
  ev.events = EPOLLIN;
  ev.data.ptr = NULL;
  if (epoll_ctl(t->epollfd, EPOLL_CTL_ADD, t->listen_sock, &ev) != 0) {
    LOG_E(HW, "epoll_ctl(EPOLL_CTL_ADD) failed, errno(%d)\n", errno);
    return -1;
  }
  return 0;
}

static int client_try_connect(const char *host, uint16_t port)
{
  int sock = -1;
  int s;
  struct addrinfo *result = NULL;
  struct addrinfo *rp = NULL;

  char dport[6];
  snprintf(dport, sizeof(dport), "%d", port);

  struct addrinfo hints = {
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
  };

  s = getaddrinfo(host, dport, &hints, &result);
  if (s != 0) {
    LOG_E(HW, "getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == -1) {
      continue;
    }

    if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(sock);
    sock = -1;
  }

  freeaddrinfo(result);

  return sock;
}

static int startClient(openair0_device_t *device)
{
  rfsimulator_state_t *t = static_cast<rfsimulator_state_t *>(device->priv);
  t->role = SIMU_ROLE_CLIENT;
  int sock;

  while (true) {
    LOG_I(HW, "Trying to connect to %s:%d\n", t->ip, t->port);
    sock = client_try_connect(t->ip, t->port);

    if (sock > 0) {
      LOG_I(HW, "Connection to %s:%d established\n", t->ip, t->port);
      break;
    }

    LOG_I(HW, "connect() to %s:%d failed, errno(%d)\n", t->ip, t->port, errno);
    sleep(1);
  }

  if (setblocking(sock, notBlocking) == -1) {
    return -1;
  }
  buffer_t *b = allocCirBuf(t, sock);
  if (!b)
    return -1;
  // read a 1 sample block to initialize the current time
  bool have_to_wait;
  do {
    have_to_wait = true;
    flushInput(t, 3, true);
    if (b->lastReceivedTS)
      have_to_wait = false;
  } while (have_to_wait);
  if (b->lastReceivedTS > 0)
    b->lastReceivedTS--;
  t->nextRxTstamp = b->lastReceivedTS;
  LOG_D(HW, "Client got first timestamp: starting at %lu\n", t->nextRxTstamp);
  if (b->channel_model)
    b->channel_model->start_TS = t->nextRxTstamp;
  return 0;
}

static int rfsimulator_write_internal(rfsimulator_state_t *t,
                                      openair0_timestamp_t timestamp,
                                      void ***samplesVoid,
                                      int nsamps,
                                      int nbAnt,
                                      std::vector<int> tx_beams,
                                      int flags)
{
  mutexlock(t->Sockmutex);
  LOG_D(HW, "Sending %d samples at time: %ld, nbAnt %d\n", nsamps, timestamp, nbAnt);

  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];

    if (b->conn_sock >= 0) {
      samplesBlockHeader_t header = {(uint32_t)nsamps, (uint32_t)nbAnt, (uint64_t)timestamp, 0, 0, beams_to_beam_map(tx_beams)};
      fullwrite(b->conn_sock, &header, sizeof(header), t);
      int num_beams = tx_beams.size();
      // Send beams in order of beam index. This is required for beam_map to work correctly on the receiver side.
      std::vector<size_t> indices(tx_beams.size());
      std::iota(indices.begin(), indices.end(), 0);
      std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) { return tx_beams[i] < tx_beams[j]; });

      AssertFatal(num_beams > 0, "Must set at least one bit in beam_map\n");
      for (int beam = 0; beam < num_beams; beam++) {
        for (int a = 0; a < nbAnt; a++) {
          sample_t *in = (sample_t *)samplesVoid[indices[beam]][a];
          fullwrite(b->conn_sock, (void *)in, sampleToByte(nsamps, 1), t);
        }
      }
    }
  }

  if (t->lastWroteTS > timestamp)
    LOG_W(HW, "Not supported to send Tx out of order %lu, %lu\n", t->lastWroteTS, timestamp);

  if ((flags != TX_BURST_START) && (flags != TX_BURST_START_AND_END) && (t->lastWroteTS < timestamp))
    LOG_W(HW,
          "Gap in writing to USRP: last written %lu, now %lu, gap %lu\n",
          t->lastWroteTS,
          timestamp,
          timestamp - t->lastWroteTS);

  t->lastWroteTS = timestamp + nsamps;
  mutexunlock(t->Sockmutex);

  LOG_D(HW,
        "Sent %d samples at time: %ld->%ld, energy in first antenna: %d\n",
        nsamps,
        timestamp,
        timestamp + nsamps,
        signal_energy(static_cast<int32_t *>(samplesVoid[0][0]), nsamps));

  /* trace only first antenna */
  T(T_USRP_TX_ANT0, T_INT(timestamp), T_BUFFER(samplesVoid[0][0], (int)sampleToByte(nsamps, 1)));

  return nsamps;
}

static int rfsimulator_write_beams(openair0_device_t *device,
                                   openair0_timestamp_t timestamp,
                                   void ***samplesVoid,
                                   int nsamps,
                                   int nbAnt,
                                   int num_beams,
                                   int flags)
{
  timestamp -= device->openair0_cfg->command_line_sample_advance;
  int nsamps_initial = nsamps;
  rfsimulator_state_t *t = static_cast<rfsimulator_state_t *>(device->priv);
  void *samples[num_beams][nbAnt];
  void **samples_ptr[num_beams];
  for (int beam = 0; beam < num_beams; beam++) {
    samples_ptr[beam] = samples[beam];
    for (int aatx = 0; aatx < nbAnt; aatx++) {
      samples[beam][aatx] = samplesVoid[beam][aatx];
    }
  }
  while (nsamps > 0) {
    uint32_t nsamps_beam_map;
    std::vector<int> beams = get_beams(&t->beam_ctrl->tx, timestamp, nsamps, &nsamps_beam_map);
    rfsimulator_write_internal(t, timestamp, samples_ptr, nsamps_beam_map, nbAnt, beams, flags);
    for (int beam = 0; beam < num_beams; beam++) {
      for (int aatx = 0; aatx < nbAnt; aatx++) {
        char *ptr = (char *)samples_ptr[beam][aatx];
        samples_ptr[beam][aatx] = (void *)(ptr + nsamps_beam_map * sizeof(sample_t));
      }
    }
    timestamp += nsamps_beam_map;
    nsamps -= nsamps_beam_map;
  }
  clear_beam_queue(&t->beam_ctrl->tx, timestamp + nsamps);
  return nsamps_initial;
}

static int rfsimulator_write(openair0_device_t *device, openair0_timestamp_t timestamp, void **buff, int nsamps, int cc, int flags)
{
  void **tmp = buff;
  return rfsimulator_write_beams(device, timestamp, &tmp, nsamps, cc, 1, flags);
}

static bool add_client(rfsimulator_state_t *t)
{
  struct sockaddr_storage sa = {0};
  socklen_t socklen = sizeof(sa);
  int conn_sock = accept(t->listen_sock, (struct sockaddr *)&sa, &socklen);
  if (conn_sock == -1) {
    LOG_E(HW, "accept() failed, errno(%d)\n", errno);
    return false;
  }
  if (setblocking(conn_sock, notBlocking)) {
    return false;
  }
  mutexlock(t->Sockmutex);
  buffer_t *new_buf = allocCirBuf(t, conn_sock);
  if (new_buf == NULL) {
    mutexunlock(t->Sockmutex);
    return false;
  }
  new_buf->lastReceivedTS = t->lastWroteTS;
  char ip[INET6_ADDRSTRLEN];
  getnameinfo((struct sockaddr *)&sa, socklen, ip, sizeof(ip), NULL, 0, NI_NUMERICHOST);
  uint16_t port = ((struct sockaddr_in *)&sa)->sin_port;
  LOG_I(HW, "Client connects from %s:%d\n", ip, port);
  c16_t v = {0};
  void *samplesVoid[t->tx_num_channels];
  for (int i = 0; i < t->tx_num_channels; i++)
    samplesVoid[i] = (void *)&v;
  samplesBlockHeader_t header = {1, (uint32_t)t->tx_num_channels, (uint64_t)t->lastWroteTS, 0, 0, 1};
  fullwrite(conn_sock, &header, sizeof(header), t);
  fullwrite(conn_sock, samplesVoid, sampleToByte(1, t->tx_num_channels), t);

  if (new_buf->channel_model)
    new_buf->channel_model->start_TS = t->lastWroteTS;
  mutexunlock(t->Sockmutex);
  return true;
}

static void process_recv_header(rfsimulator_state_t *t, buffer_t *b, bool first_time)
{
  b->headerMode = false; // We got the header
  AssertFatal(b->th.nbAnt != 0, "Number of antennas not set\n");
  if (b->nbAnt != b->th.nbAnt) {
    LOG_A(HW, "RFsim: Number of antennas changed from %d to %d\n", b->nbAnt, b->th.nbAnt);
    b->nbAnt = b->th.nbAnt;
  }
  if (first_time) {
    b->lastReceivedTS = b->th.timestamp;
    b->trashingPacket = true;
  } else {
    if (b->lastReceivedTS < (int64_t)b->th.timestamp) {
      int nbAnt = b->th.nbAnt;
      if (!nbAnt)
        LOG_E(HW, "rfsimulator receive 0 rx antennas\n");
      b->lastReceivedTS = b->th.timestamp;
    } else if (b->lastReceivedTS > (int64_t)b->th.timestamp) {
      LOG_W(HW, "Received data in past: current is %lu, new reception: %lu!\n", b->lastReceivedTS, b->th.timestamp);
      b->trashingPacket = true;
    }
  }

  int num_beams = __builtin_popcountll(b->th.beam_map);
  AssertFatal(b->th.beam_map == 1ULL || t->beam_ctrl->enable_beams == 1,
              "The transmitter has enabled beam simulation while this receiver has not\n");
  size_t payload_sz = sampleToByte(b->th.size, b->th.nbAnt) * num_beams;
  b->packet_ptr = static_cast<rfsim_packet_t *>(calloc_or_fail(1, payload_sz + sizeof(samplesBlockHeader_t)));
  b->packet_ptr->header = b->th;
  b->transferPtr = b->packet_ptr->payload;
  b->remainToTransfer = payload_sz;
  return;
}

/**
 * @brief Combines samples from received packets into a single buffer for the rx beam
 *
 * This function processes the received_packets queue and combines the transmitted beams
 * into a single buffer for each antenna. It applies the appropriate gain and handles
 * overlapping timestamps.
 *
 *
 * @param t Pointer to the rfsimulator_state_t structure.
 * @param received_packets Reference to the queue of rfsim_packet_t pointers representing received packets.
 * @param start_timestamp The start timestamp for the combination process.
 * @param num_aatx The number of antennas to process.
 * @param num_samples The number of samples to process.
 * @param rx_beam_id Receiver configured beam id for the timestamp
 * @param samples Pointer to the output buffer where combined samples will be stored.
 * @return A vector of vectors containing the combined samples for each antenna.
 */
static void combine_received_beams(rfsimulator_state_t *t,
                                   std::queue<rfsim_packet_t *> &received_packets,
                                   uint64_t start_timestamp,
                                   int num_aatx,
                                   size_t num_samples,
                                   int rx_beam_id,
                                   c16_t **samples)
{
  // Assume received_packets is ordered by timestamp
  std::queue<rfsim_packet_t *> packets_copy = received_packets;
  while (!packets_copy.empty()) {
    rfsim_packet_t *pkt = packets_copy.front();
    if (pkt->header.timestamp + pkt->header.size <= start_timestamp) {
      // This packet is before the start timestamp, discard it
      packets_copy.pop();
      continue;
    }
    if (pkt->header.timestamp > start_timestamp + num_samples) {
      // This packet is after the end of the buffer, stop processing
      break;
    }

    // The beams transmitted in are ordered by beam index
    std::vector<int> tx_beams = beam_map_to_beams(pkt->header.beam_map);
    for (uint beam = 0; beam < tx_beams.size(); beam++) {
      float gain_dB = get_rx_gain_db(t, rx_beam_id, tx_beams[beam]);
      float gain_linear = powf(10, gain_dB / 20.0);
      uint64_t overlap_start = std::max(start_timestamp, pkt->header.timestamp);
      uint64_t overlap_end = std::min(start_timestamp + num_samples, pkt->header.timestamp + pkt->header.size);
      int write_start_idx = overlap_start - start_timestamp;
      int write_end_idx = overlap_end - start_timestamp;
      int read_start_idx = overlap_start - pkt->header.timestamp;
      for (int aatx = 0; aatx < num_aatx; aatx++) {
        c16_t *buffer = (c16_t *)pkt->payload;
        c16_t *tx_ant_buffer_in = &buffer[(num_aatx * beam + aatx) * pkt->header.size + read_start_idx];
        if (beam == 0) {
          // For the first beam, we can directly copy the samples
          if (gain_dB == 0.0f) {
            // If gain is 0 dB, we can use memcpy for efficiency
            memcpy(&samples[aatx][write_start_idx], tx_ant_buffer_in, (write_end_idx - write_start_idx) * sizeof(c16_t));
          } else {
            for (int s = write_start_idx; s < write_end_idx; s++) {
              samples[aatx][s].r = tx_ant_buffer_in->r * gain_linear;
              samples[aatx][s].i = tx_ant_buffer_in->i * gain_linear;
              tx_ant_buffer_in++;
            }
          }
        } else {
          // For subsequent beams, we need to ensure we accumulate the samples
          if (gain_dB == 0.0f) {
            for (int s = write_start_idx; s < write_end_idx; s++) {
              samples[aatx][s].r += tx_ant_buffer_in->r;
              samples[aatx][s].i += tx_ant_buffer_in->i;
              tx_ant_buffer_in++;
            }
          } else {
            for (int s = write_start_idx; s < write_end_idx; s++) {
              samples[aatx][s].r += tx_ant_buffer_in->r * gain_linear;
              samples[aatx][s].i += tx_ant_buffer_in->i * gain_linear;
              tx_ant_buffer_in++;
            }
          }
        }
      }
    }
    packets_copy.pop();
  }
}

static bool flushInput(rfsimulator_state_t *t, int timeout, bool first_time)
{
  // Process all incoming events on sockets
  // store the data in lists
  struct epoll_event events[MAX_FD_RFSIMU] = {{0}};
  int nfds = epoll_wait(t->epollfd, events, MAX_FD_RFSIMU, timeout);

  if (nfds == -1) {
    if (!(errno == EINTR || errno == EAGAIN))
      LOG_W(HW, "epoll_wait() failed, errno(%d)\n", errno);
    return false;
  }

  for (int nbEv = 0; nbEv < nfds; ++nbEv) {
    buffer_t *b = static_cast<buffer_t *>(events[nbEv].data.ptr);

    if (events[nbEv].events & EPOLLIN && b == NULL) {
      bool ret = add_client(t);
      if (!ret)
        return ret;
      continue;
    } else {
      if (events[nbEv].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
        socketError(t, b);
        continue;
      }
    }

    if (b->conn_sock == -1) {
      LOG_E(HW, "Received data on not connected socket %d\n", events[nbEv].data.fd);
      continue;
    }

    ssize_t sz = recv(b->conn_sock, b->transferPtr, b->remainToTransfer, MSG_DONTWAIT);
    if (sz <= 0) {
      if (sz < 0 && errno != EAGAIN)
        LOG_E(HW, "recv() failed, errno(%d)\n", errno);
      continue;
    }
    LOG_D(HW, "Socket rcv %zd bytes\n", sz);
    b->remainToTransfer -= sz;
    b->transferPtr += sz;
    if (b->remainToTransfer == 0) {
      if (b->headerMode)
        process_recv_header(t, b, first_time);
      else {
        LOG_D(HW, "UEsock: %d Completed block reception: %ld\n", b->conn_sock, b->lastReceivedTS);
        b->headerMode = true;
        b->transferPtr = (char *)&b->th;
        b->remainToTransfer = sizeof(samplesBlockHeader_t);

        if (!b->trashingPacket) {
          b->lastReceivedTS = b->th.timestamp + b->th.size;
          LOG_D(HW, "UEsock: %d Set b->lastReceivedTS %ld\n", b->conn_sock, b->lastReceivedTS);
          b->received_packets.emplace(b->packet_ptr);
        } else {
          free(b->packet_ptr);
        }
        b->packet_ptr = NULL;
        b->trashingPacket = false;
      }
    }
  }
  return nfds > 0;
}

static void rfsimulator_read_internal(rfsimulator_state_t *t,
                                      c16_t **samples,
                                      openair0_timestamp_t timestamp,
                                      int nsamps,
                                      int nbAnt,
                                      int rx_beam_id,
                                      bool is_first_beam)
{
  cf_t temp_array[nbAnt][nsamps];
  bool channel_modelling = false;
  // Add all input nodes signal in the output buffer
  bool is_first_peer = true;
  for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
    buffer_t *ptr = &t->buf[sock];

    if (ptr->conn_sock != -1 && !ptr->received_packets.empty()) {
      AssertFatal(ptr->nbAnt != 0, "Number of antennas not set\n");
      bool reGenerateChannel = false;

      // fixme: when do we regenerate
      //  it seems legacy behavior is: never in UL, each frame in DL
      if (reGenerateChannel)
        random_channel(ptr->channel_model, 0);

      if (ptr->channel_model != NULL) { // apply a channel model
        if (!channel_modelling) {
          memset(temp_array, 0, sizeof(temp_array));
          channel_modelling = true;
        }
        const uint64_t channel_offset = ptr->channel_model->channel_offset;
        const uint64_t channel_length = ptr->channel_model->channel_length;
        std::vector<std::vector<c16_t>> ant_buffers(ptr->nbAnt, std::vector<c16_t>(nsamps + channel_length - 1, {0, 0}));
        c16_t *input[ant_buffers.size()];
        for (uint aatx = 0; aatx < ant_buffers.size(); aatx++) {
          input[aatx] = ant_buffers[aatx].data();
        }

        combine_received_beams(t,
                               ptr->received_packets,
                               timestamp - channel_offset - (channel_length - 1),
                               ptr->nbAnt,
                               nsamps + channel_length - 1,
                               rx_beam_id,
                               input);

        for (int aarx = 0; aarx < nbAnt; aarx++) {
          rxAddInput(input, temp_array[aarx], aarx, ptr->channel_model, nsamps);
        }
      } else {
        if (is_first_beam && is_first_peer && (ptr->nbAnt == 1 && nbAnt == 1)) {
          // optimization: The buffer is uninitialized so samples can be written directly in the buffer
          combine_received_beams(t, ptr->received_packets, timestamp - t->chan_offset, 1, nsamps, rx_beam_id, samples);
        } else {
          std::vector<std::vector<c16_t>> ant_buffers(ptr->nbAnt, std::vector<c16_t>(nsamps, {0, 0}));
          c16_t *input[ant_buffers.size()];
          for (uint aatx = 0; aatx < ant_buffers.size(); aatx++) {
            input[aatx] = ant_buffers[aatx].data();
          }
          combine_received_beams(t, ptr->received_packets, timestamp - t->chan_offset, ptr->nbAnt, nsamps, rx_beam_id, input);
          for (int aarx = 0; aarx < nbAnt; aarx++) {
            double H_awgn_mimo_coeff[ant_buffers.size()];
            for (int aatx = 0; aatx < (int)ant_buffers.size(); aatx++) {
              uint32_t ant_diff = std::abs(aatx - aarx);
              H_awgn_mimo_coeff[aatx] = ant_diff ? (0.2 / ant_diff) : 1.0;
            }

            for (uint aatx = 0; aatx < ant_buffers.size(); aatx++) {
              for (int i = 0; i < nsamps; i++) {
                samples[aarx][i].r += ant_buffers[aatx][i].r * H_awgn_mimo_coeff[aatx];
                samples[aarx][i].i += ant_buffers[aatx][i].i * H_awgn_mimo_coeff[aatx];
              }
            }
          }
        }
      }
    }
    is_first_peer = false;
  }

  bool apply_global_noise = get_noise_power_dBFS() != INVALID_DBFS_VALUE;
  if (apply_global_noise) {
    if (!channel_modelling) {
      memset(temp_array, 0, sizeof(temp_array));
      channel_modelling = true;
    }
    int16_t noise_power = (int16_t)(32767.0 / powf(10.0, .05 * -get_noise_power_dBFS()));
    for (int a = 0; a < nbAnt; a++) {
      for (int i = 0; i < nsamps; i++) {
        temp_array[a][i].r += noise_power * gaussZiggurat(0.0, 1.0);
        temp_array[a][i].i += noise_power * gaussZiggurat(0.0, 1.0);
      }
    }
  }

  if (channel_modelling) {
    for (int a = 0; a < nbAnt; a++) {
      for (int i = 0; i < nsamps; i++) {
        samples[a][i].r += lroundf(temp_array[a][i].r);
        samples[a][i].i += lroundf(temp_array[a][i].i);
      }
    }
  }
}

static int rfsimulator_read_beams(openair0_device_t *device,
                                  openair0_timestamp_t *ptimestamp,
                                  void ***samplesVoid,
                                  int nsamps,
                                  int nbAnt,
                                  int num_beams)
{
  rfsimulator_state_t *t = static_cast<rfsimulator_state_t *>(device->priv);
  LOG_D(HW,
        "Enter rfsimulator_read, expect %d samples, will release at TS: %ld, nbAnt %d\n",
        nsamps,
        t->nextRxTstamp + nsamps,
        nbAnt);

  // deliver data from received data
  // check if a UE is connected
  int first_sock;

  for (first_sock = 0; first_sock < MAX_FD_RFSIMU; first_sock++)
    if (t->buf[first_sock].conn_sock != -1)
      break;

  if (first_sock == MAX_FD_RFSIMU) {
    // no connected device (we are eNB, no UE is connected)
    if (t->nextRxTstamp == 0)
      LOG_I(HW, "No connected device, generating void samples...\n");

    if (!flushInput(t, t->wait_timeout, false)) {
      for (int beam = 0; beam < num_beams; beam++)
        for (int x = 0; x < nbAnt; x++)
          memset(samplesVoid[beam][x], 0, sampleToByte(nsamps, 1));

      t->nextRxTstamp += nsamps;

      if (((t->nextRxTstamp / nsamps) % 100) == 0)
        LOG_D(HW, "No UE, Generating void samples for Rx: %ld\n", t->nextRxTstamp);

      *ptimestamp = t->nextRxTstamp - nsamps;
      return nsamps;
    }
  } else {
    bool have_to_wait;
    do {
      have_to_wait = false;
      buffer_t *b = NULL;
      for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
        b = &t->buf[sock];
        if (b->conn_sock != -1 && (t->nextRxTstamp + nsamps) > b->lastReceivedTS) {
          have_to_wait = true;
          break;
        }
      }

      if (have_to_wait) {
        LOG_D(HW,
              "Waiting on socket, current last ts: %ld, expected at least : %ld\n",
              b->lastReceivedTS,
              t->nextRxTstamp + nsamps);
        flushInput(t, 3, false);
      }
    } while (have_to_wait);
  }

  struct timespec start_time;
  int ret = clock_gettime(CLOCK_REALTIME, &start_time);
  AssertFatal(ret == 0, "clock_gettime() failed: errno %d, %s\n", errno, strerror(errno));

  for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
    buffer_t *ptr = &t->buf[sock];

    if (ptr->conn_sock != -1 && ptr->channel_model != NULL) {
      update_channel_model(ptr->channel_model, nsamps, t->nextRxTstamp);
    }
  }

  if (t->poll_telnetcmdq)
    t->poll_telnetcmdq(t->telnetcmd_qid, t);

  // Clear the output buffer
  for (int beam = 0; beam < num_beams; beam++)
    for (int a = 0; a < nbAnt; a++)
      memset(samplesVoid[beam][a], 0, sampleToByte(nsamps, 1));

  openair0_timestamp_t timestamp = t->nextRxTstamp;
  int nsamps_to_process = nsamps;
  while (nsamps_to_process > 0) {
    uint32_t nsamps_beam_map;
    std::vector<int> rx_beams = get_beams(&t->beam_ctrl->tx, timestamp, nsamps_to_process, &nsamps_beam_map);
    if ((int)rx_beams.size() != num_beams) {
      LOG_D(HW,
            "Number of beams does not match application request num_beams %d, beam_map beams %lu\n",
            num_beams,
            rx_beams.size());
    }
    for (int beam = 0; beam < num_beams && beam < (int)rx_beams.size(); beam++) {
      c16_t *samples_beam[nbAnt];
      for (int i = 0; i < nbAnt; i++) {
        samples_beam[i] = (c16_t *)samplesVoid[beam][i] + timestamp - t->nextRxTstamp;
      }
      rfsimulator_read_internal(t, samples_beam, timestamp, nsamps_beam_map, nbAnt, rx_beams[beam], beam == 0);
    }
    timestamp += nsamps_beam_map;
    nsamps_to_process -= nsamps_beam_map;
  }

  struct timespec end_time;
  ret = clock_gettime(CLOCK_REALTIME, &end_time);
  AssertFatal(ret == 0, "clock_gettime() failed: errno %d, %s\n", errno, strerror(errno));
  double diff_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
  static double average = 0.0;
  average = (average * 0.98) + (nsamps / (diff_ns / 1e9) * 0.02);
  static int calls = 0;
  if (calls++ % 10000 == 0) {
    LOG_D(HW, "Rfsimulator: velocity %.2f Msps, realtime requirements %.2f Msps\n", average / 1e6, t->sample_rate / 1e6);
  }

  *ptimestamp = t->nextRxTstamp; // return the time of the first sample
  t->nextRxTstamp += nsamps;
  clear_beam_queue(&t->beam_ctrl->rx, t->nextRxTstamp);
  LOG_D(HW,
        "Rx to upper layer: %d from %ld to %ld, energy in first antenna %d\n",
        nsamps,
        *ptimestamp,
        t->nextRxTstamp,
        signal_energy(static_cast<int32_t *>(samplesVoid[0][0]), nsamps));

  /* trace only first antenna */
  T(T_USRP_RX_ANT0, T_INT(t->nextRxTstamp), T_BUFFER(samplesVoid[0], (int)sampleToByte(nsamps, 1)));

  for (int sock = 0; sock < MAX_FD_RFSIMU; sock++) {
    buffer_t *ptr = &t->buf[sock];

    if (ptr->conn_sock != -1 && !ptr->received_packets.empty()) {
      openair0_timestamp_t timestamp_to_free = t->nextRxTstamp - 1;
      if (ptr->channel_model) {
        timestamp_to_free -=
            (ptr->channel_model->channel_length - 1) + std::max(ptr->channel_model->channel_offset, t->chan_offset);
      } else {
        timestamp_to_free -= t->chan_offset;
      }
      clear_old_packets(ptr->received_packets, timestamp_to_free);
    }
  }

  return nsamps;
}

static int rfsimulator_read(openair0_device_t *device, openair0_timestamp_t *ptimestamp, void **samplesVoid, int nsamps, int nbAnt)
{
  return rfsimulator_read_beams(device, ptimestamp, &samplesVoid, nsamps, nbAnt, 1);
}

static int rfsimulator_get_stats(openair0_device_t *device)
{
  UNUSED(device);
  return 0;
}
static int rfsimulator_reset_stats(openair0_device_t *device)
{
  UNUSED(device);
  return 0;
}
static void rfsimulator_end(openair0_device_t *device)
{
  rfsimulator_state_t *s = static_cast<rfsimulator_state_t *>(device->priv);
  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &s->buf[i];
    if (b->conn_sock >= 0)
      removeCirBuf(s, b);
  }
  clear_beam_queue(&s->beam_ctrl->tx, INT64_MAX);
  clear_beam_queue(&s->beam_ctrl->rx, INT64_MAX);
  delete s->beam_ctrl;
  close(s->epollfd);
  free(s);
}

static void stopServer(openair0_device_t *device)
{
  rfsimulator_state_t *t = (rfsimulator_state_t *)device->priv;
  DevAssert(t != NULL);
  close(t->listen_sock);
  rfsimulator_end(device);
}

static int rfsimulator_stop(openair0_device_t *device)
{
  UNUSED(device);
  return 0;
}
static int rfsimulator_set_freq(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  rfsimulator_state_t *s = static_cast<rfsimulator_state_t *>(device->priv);
  s->rx_freq = openair0_cfg->rx_freq[0];
  return 0;
}
static int rfsimulator_set_gains(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  UNUSED(device);
  UNUSED(openair0_cfg);
  return 0;
}
static int rfsimulator_write_init(openair0_device_t *device)
{
  UNUSED(device);
  return 0;
}

extern "C" __attribute__((__visibility__("default"))) int device_init(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  // to change the log level, use this on command line
  // --log_config.hw_log_level debug
  rfsimulator_state_t *rfsimulator = static_cast<rfsimulator_state_t *>(calloc(sizeof(rfsimulator_state_t), 1));
  // initialize channel simulation
  rfsimulator->ru_id = openair0_cfg->ru_id;
  rfsimulator->tx_num_channels = openair0_cfg->tx_num_channels;
  rfsimulator->rx_num_channels = openair0_cfg->rx_num_channels;
  rfsimulator->sample_rate = openair0_cfg->sample_rate;
  rfsimulator->rx_freq = openair0_cfg->rx_freq[0];
  rfsimulator->tx_bw = openair0_cfg->tx_bw;
  rfsimulator->beam_ctrl = new rfsim_beam_ctrl_t;
  rfsimulator_readconfig(rfsimulator);
  if (rfsimulator->prop_delay_ms > 0.0)
    rfsimulator->chan_offset = ceil(rfsimulator->sample_rate * rfsimulator->prop_delay_ms / 1000);
  if (rfsimulator->chan_offset != 0) {
    rfsimulator->prop_delay_ms = rfsimulator->chan_offset * 1000 / rfsimulator->sample_rate;
    LOG_I(HW, "propagation delay %f ms, %lu samples\n", rfsimulator->prop_delay_ms, rfsimulator->chan_offset);
  }
  mutexinit(rfsimulator->Sockmutex);
  LOG_I(HW,
        "Running as %s\n",
        rfsimulator->role == SIMU_ROLE_SERVER ? "server waiting opposite rfsimulators to connect"
                                              : "client: will connect to a rfsimulator server side");
  device->trx_start_func = rfsimulator->role == SIMU_ROLE_SERVER ? startServer : startClient;
  device->trx_get_stats_func = rfsimulator_get_stats;
  device->trx_reset_stats_func = rfsimulator_reset_stats;
  device->trx_end_func = rfsimulator->role == SIMU_ROLE_SERVER ? stopServer : rfsimulator_end;
  device->trx_stop_func = rfsimulator_stop;
  device->trx_set_freq_func = rfsimulator_set_freq;
  device->trx_set_gains_func = rfsimulator_set_gains;
  device->trx_write_func = rfsimulator_write;
  device->trx_read_func = rfsimulator_read;
  if (rfsimulator->beam_ctrl->enable_beams) {
    device->trx_write_beams_func = rfsimulator_write_beams;
    device->trx_read_beams_func = rfsimulator_read_beams;
  }
  /* let's pretend to be a b2x0 */
  device->type = RFSIMULATOR;
  device->host_type = RAU_HOST;
  openair0_cfg->rx_gain[0] = 0;
  device->openair0_cfg = openair0_cfg;
  device->priv = rfsimulator;
  device->trx_write_init = rfsimulator_write_init;
  device->trx_set_beams = rfsimulator_set_beams;
  device->trx_set_beams2 = rfsimulator_set_beams_vector;

  for (int i = 0; i < MAX_FD_RFSIMU; i++)
    rfsimulator->buf[i].conn_sock = -1;
  rfsimulator->next_buf = 0;

  AssertFatal((rfsimulator->epollfd = epoll_create1(0)) != -1, "epoll_create1() failed, errno(%d)", errno);
  // we need to call randominit() for telnet server (use gaussdouble=>uniformrand)
  randominit();
  set_taus_seed(0);
  /* look for telnet server, if it is loaded, add the channel modeling commands to it */
  add_telnetcmd_func_t addcmd = (add_telnetcmd_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_ADDCMD_FNAME);

  if (addcmd != NULL) {
    rfsimulator->poll_telnetcmdq = (poll_telnetcmdq_func_t)get_shlibmodule_fptr("telnetsrv", TELNET_POLLCMDQ_FNAME);
    addcmd("rfsimu", rfsimu_vardef, rfsimu_cmdarray);

    for (int i = 0; rfsimu_cmdarray[i].cmdfunc != NULL; i++) {
      if (rfsimu_cmdarray[i].qptr != NULL) {
        rfsimulator->telnetcmd_qid = rfsimu_cmdarray[i].qptr;
        break;
      }
    }
  }

  /* write on a socket fails if the other end is closed and we get SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    perror("SIGPIPE");
    exit(1);
  }

  return 0;
}
