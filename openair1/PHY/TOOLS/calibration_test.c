/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include "openair1/PHY/defs_common.h"
#include <radio/COMMON/common_lib.h>
#include <executables/softmodem-common.h>
#include <openair1/PHY/TOOLS/calibration_scope.h>
#include "nfapi/oai_integration/vendor_ext.h"
#include "common/config/config_userapi.h"


int oai_exit=false;
unsigned int mmapped_dma=0;
uint32_t timing_advance;
int8_t threequarter_fs;
uint64_t downlink_frequency[MAX_NUM_CCs][4];
int64_t uplink_frequency_offset[MAX_NUM_CCs][4];
int cpu_meas_enabled;
THREAD_STRUCT thread_struct;
#include <executables/nr-softmodem.h>

int read_recplayconfig(recplay_conf_t **recplay_conf, recplay_state_t **recplay_state) {return 0;}
void nfapi_setmode(nfapi_mode_t nfapi_mode) {}
void set_taus_seed(unsigned int seed_init){};
int main(int argc, char **argv) {
  ///static configuration for NR at the moment
  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == NULL) {
    fprintf(stderr, "configuration module init failed\n");
    return -1;
  }
  set_softmodem_sighandler();
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  logInit();

  CONFIG_SETRTFLAG(CONFIG_NOEXITONHELP);
  get_common_options(uniqCfg);
  CONFIG_CLEARRTFLAG(CONFIG_NOEXITONHELP);
  lock_memory_to_ram();
    
  int sampling_rate=30.72e6;
  int DFT=2048;
  int TxAdvanceInDFTSize=12;
  int antennas=1;
  uint64_t freq=3619.200e6;
  int rxGain=90;
  int txGain=90;
  int filterBand=40e6;
  char * usrp_addrs="type=b200";

  openair0_config_t openair0_cfg= {
    //! the sample rate for both transmit and receive.
    .sample_rate=sampling_rate,
    //! samples per packet on the fronthaul interface
    .samples_per_packet=1024,
    //! number of RX channels (=RX antennas)
    .rx_num_channels=antennas,
    //! number of TX channels (=TX antennas)
    .tx_num_channels=antennas,
    //! \brief Center frequency in Hz for RX.
    //! index: [0..rx_num_channels[
    .rx_freq={freq,freq,freq,freq},
    //! \brief Center frequency in Hz for TX.
    //! index: [0..rx_num_channels[ !!! see lte-ue.c:427 FIXME iterates over rx_num_channels
    .tx_freq={freq,freq,freq,freq},
    //! \brief Gain for RX in dB.
    //! index: [0..rx_num_channels]
    .rx_gain={rxGain,rxGain,rxGain,rxGain},
    //! gain for TX in dB
    .tx_gain={txGain,txGain,txGain,txGain},
    //! RX bandwidth in Hz
    .rx_bw=filterBand,
    //! TX bandwidth in Hz
    .tx_bw=filterBand,
    //! clock source
    .clock_source=external,//internal gpsdo external
    //! timing_source
    .time_source=internal, //internal gpsdo external
    //! Manual SDR IP address
    .sdr_addrs=usrp_addrs,
  };
  //-----------------------
  openair0_device_t rfdevice= {
    /*!brief Type of this device */
    .type=NONE_DEV,
    /*!brief Transport protocol type that the device supports (in case I/Q samples need to be transported) */
    .transp_type=NONE_TP,
    /*!brief Type of the device's host (RAU/RRU) */
    .host_type=MIN_HOST_TYPE,
    /* !brief RF frontend parameters set by application */
    .openair0_cfg=NULL, //set by device_init
    //! record player data, definition in record_player.h
    .recplay_state=NULL,
    /*!brief Can be used by driver to hold internal structure*/
    .priv=NULL,
    /* Functions API, which are called by the application*/
    /*! \brief Called to start the transceiver. Return 0 if OK, < 0 if error
        @param device pointer to the device structure specific to the RF hardware target
    */
    .trx_start_func=NULL,

    /*! \brief Called to configure the device
         @param device pointer to the device structure specific to the RF hardware target
     */
    .trx_config_func=NULL,

    /*! \brief Called to send a request message between RAU-RRU on control port
        @param device pointer to the device structure specific to the RF hardware target
        @param msg pointer to the message structure passed between RAU-RRU
        @param msg_len length of the message
    */
    .trx_ctlsend_func=NULL,

    /*! \brief Called to receive a reply  message between RAU-RRU on control port
        @param device pointer to the device structure specific to the RF hardware target
        @param msg pointer to the message structure passed between RAU-RRU
        @param msg_len length of the message
    */
    .trx_ctlrecv_func=NULL,

    /*! \brief Called to send samples to the RF target
          @param device pointer to the device structure specific to the RF hardware target
        @param timestamp The timestamp at whicch the first sample MUST be sent
        @param buff Buffer which holds the samples (2 dimensional)
        @param nsamps number of samples to be sent
        @param number of antennas
        @param flags flags must be set to TRUE if timestamp parameter needs to be applied
    */
    .trx_write_func=NULL,

    /*! \brief Called to send samples to the RF target
        @param device pointer to the device structure specific to the RF hardware target
        @param timestamp The timestamp at whicch the first sample MUST be sent
        @param buff Buffer which holds the samples (1 dimensional)
        @param nsamps number of samples to be sent
        @param antenna_id index of the antenna if the device has multiple anteannas
        @param flags flags must be set to TRUE if timestamp parameter needs to be applied
    */
    .trx_write_func2=NULL,

    /*! \brief Receive samples from hardware.
     * Read \ref nsamps samples from each channel to buffers. buff[0] is the array for
     * the first channel. *ptimestamp is the time at which the first sample
     * was received.
     * \param device the hardware to use
     * \param[out] ptimestamp the time at which the first sample was received.
     * \param[out] buff An array of pointers to buffers for received samples. The buffers must be large enough to hold the number of samples \ref nsamps.
     * \param nsamps Number of samples. One sample is 2 byte I + 2 byte Q => 4 byte.
     * \param num_antennas number of antennas from which to receive samples
     * \returns the number of sample read
     */

    .trx_read_func=NULL,

    /*! \brief Receive samples from hardware, this version provides a single antenna at a time and returns.
     * Read \ref nsamps samples from each channel to buffers. buff[0] is the array for
     * the first channel. *ptimestamp is the time at which the first sample
     * was received.
     * \param device the hardware to use
     * \param[out] ptimestamp the time at which the first sample was received.
     * \param[out] buff A pointers to a buffer for received samples. The buffer must be large enough to hold the number of samples \ref nsamps.
     * \param nsamps Number of samples. One sample is 2 byte I + 2 byte Q => 4 byte.
     * \param antenna_id Index of antenna from which samples were received
     * \returns the number of sample read
     */
    .trx_read_func2=NULL,

    /*! \brief print the device statistics
     * \param device the hardware to use
     * \returns  0 on success
     */
    /*! \brief print the device statistics
       * \param device the hardware to use
       * \returns  0 on success
       */
    .trx_get_stats_func=NULL,

    /*! \brief Reset device statistics
     * \param device the hardware to use
     * \returns 0 in success
     */
    .trx_reset_stats_func=NULL,

    /*! \brief Terminate operation of the transceiver -- free all associated resources
     * \param device the hardware to use
     */
    .trx_end_func=NULL,

    /*! \brief Stop operation of the transceiver
     */
    .trx_stop_func=NULL,

    /* Functions API related to UE*/

    /*! \brief Set RX feaquencies
     * \param device the hardware to use
     * \param openair0_cfg RF frontend parameters set by application
     * \returns 0 in success
     */
    .trx_set_freq_func=NULL,

    /*! \brief Set gains
     * \param device the hardware to use
     * \param openair0_cfg RF frontend parameters set by application
     * \returns 0 in success
     */
    .trx_set_gains_func=NULL,

    /*! \brief RRU Configuration callback
     * \param idx RU index
     * \param arg pointer to capabilities or configuration
     */
    .configure_rru=NULL,
    /*! \brief Pointer to generic RRU private information
       */
    .thirdparty_priv=NULL,
    .thirdparty_init=NULL,
    /*! \brief Callback for Third-party RRU Cleanup routine
       \param device the hardware configuration to use
     */
    .thirdparty_cleanup=NULL,

    /*! \brief Callback for Third-party start streaming routine
       \param device the hardware configuration to use
     */
    .thirdparty_startstreaming=NULL,

    /*! \brief RRU Configuration callback
     * \param idx RU index
     * \param arg pointer to capabilities or configuration
     */
    .trx_write_init=NULL,
    /* \brief Get internal parameter
     * \param id parameter to get
     * \return a pointer to the parameter
     */
    .get_internal_parameter=NULL,
  };
  
  openair0_device_load(&rfdevice,&openair0_cfg);

  void ** samplesRx = (void **)malloc16(antennas* sizeof(c16_t *) );
  void ** samplesTx = (void **)malloc16(antennas* sizeof(c16_t *) );

  int fd=open(getenv("rftestInputFile"),O_RDONLY);
  AssertFatal(fd>=0,"%s",strerror(errno));
  
  for (int i=0; i<antennas; i++) {
    samplesRx[i] = (int32_t *)malloc16_clear( DFT*sizeof(c16_t) );
    samplesTx[i] = (int32_t *)malloc16_clear( DFT*sizeof(c16_t) );
  }

  CalibrationInitScope(samplesRx, &rfdevice);
  openair0_timestamp_t timestamp=0;
  rfdevice.trx_start_func(&rfdevice);
  
  while(!oai_exit) {
    for (int i=0; i<antennas; i++) {
      ssize_t len = read(fd, samplesTx[i], DFT*sizeof(c16_t));
      if (len < 0) {
        fprintf(stderr, "error during read(): errno %d, %s\n", errno, strerror(errno));
        exit(1);
      }
    }
    rfdevice.trx_read_func(&rfdevice, &timestamp, samplesRx, DFT, antennas);
    rfdevice.trx_write_func(&rfdevice, timestamp + TxAdvanceInDFTSize * DFT, samplesTx, DFT, antennas, 0);
  }

  return 0;
}
