/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef TUN_IF_H_
#define TUN_IF_H_

#include <stdbool.h>
#include <net/if.h>
#include <linux/if_tun.h>

/*!
 * \brief This function generates the name of the interface based on the prefix and
 * the instance id.
 *
 * \param[in,out] ifname name of the interface
 * \param[in] ifprefix prefix of the interface
 * \param[in] instance_id unique instance number
 */
int tun_generate_ifname(char *ifname, const char *ifprefix, int instance_id);

/*!
 * \brief This function generates the name of the interface based on the prefix and
 * the instance id for a UE.
 *
 * \param[in,out] ifname name of the interface
 * \param[in] flag IFF_TUN (for TUN device) or IFF_TAP (for TAP device)
 * \param[in] ifprefix prefix of the interface
 * \param[in] instance_id unique instance number
 */
int tuntap_generate_ue_ifname(char *ifname, int flag, int instance_id, int pdu_session_id);

/*!
 * \brief This function initializes the TUN interface
 * \param[in] ifname name of the interface
 * \param[in] instance_id unique instance number, used to save socket file descriptor
 */
int tun_init(const char *ifname, int instance_id);

/*!
 * \brief This function initializes the TUN interface for MBMS
 * \param[in] ifname name of the interface
 */
int tun_init_mbms(char *ifname);

/*!
 * \brief Initializes the TUN interface with an IPv4 or IPv6 address and
 * activates the interface.
 * \param[in] ifname name of the interface
 * \param[in] ipv4 IPv4 address of this interface as a string
 * \param[in] ipv6 IPv6 address of this interface as a string
 * \return true on success, otherwise false
 * \note
 * @ingroup  _nas
 */
bool tun_config(const char* ifname, const char *ipv4, const char *ipv6);

/*!
 * \brief Initializes and activates the TAP interface.
 * \param[in] ifname name of the interface
 * \return true on success, otherwise false
 * \note
 * @ingroup  _nas
 */
bool tap_config(const char* ifname);

/*!
 * \brief Setup a IPv4 rule in table (interface_id - 1 + 10000) and route to
 * force packets coming into interface back through it, and workaround
 * net.ipv4.conf.all.rp_filter=2 (strict source filtering would filter out
 * responses of packets going out through interface to another IP address not
 * in same subnet).
 * \param[in] ifname name of the interface
 * \param[in] instance_id unique instance number, used to create the table
 * \param[in] ipv4 IPv4 address of the UE
 */
void setup_ue_ipv4_route(const char* ifname, int instance_id, const char *ipv4);

/*!
 * \brief This function allocates a TUN or TAP interface
 * \param[in] flag IFF_TUN (for TUN device) or IFF_TAP (for TAP device)
 * \param[in] dev name of the interface
 * \return file descriptor of the allocated interface
 */
int tuntap_alloc(int flag, const char *dev);

/*!
 * \brief This function destroys the TUN or TAP interface
 * \param[in] dev name of the interface
 */
void tuntap_destroy(const char *dev);

/*!
 * \brief Bring a TUN/TAP interface administratively up (IOCTL SIOCSIFFLAGS, set IFF_UP).
 * \param[in] ifname name of the interface
 * \param[in] sock_fd IPv4 SOCK_DGRAM fd for ioctl (opened by the caller)
 * \return interface flags with IFF_UP set, or -1 on failure
 */
short tuntap_set_up(const char *ifname, int sock_fd);

#endif /*TUN_IF_H_*/
