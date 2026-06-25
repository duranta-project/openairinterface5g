/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>
#include <linux/netlink.h>

#include "tuntap_if.h"
#include "common/platform_constants.h"
#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

int nas_sock_fd[MAX_MOBILES_PER_ENB * 2]; // Allocated for both LTE UE and NR UE.
int nas_sock_mbms_fd;

int tuntap_alloc(int flag, const char *dev)
{
  struct ifreq ifr;
  int fd, err;

  if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    LOG_E(UTIL, "failed to open /dev/net/tun\n");
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  DevAssert(flag == IFF_TUN || flag == IFF_TAP);
  ifr.ifr_flags = flag | IFF_NO_PI;
  strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);
  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
    close(fd);
    return err;
  }
  // According to https://www.kernel.org/doc/Documentation/networking/tuntap.txt, TUNSETIFF performs string
  // formatting on ifr_name. To ensure that the generated name is what we expect, check it here.
  AssertFatal(strcmp(ifr.ifr_name, dev) == 0, "Failed to create tun interface, expected ifname %s, got %s\n", dev, ifr.ifr_name);

  err = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (err == -1) {
    close(fd);
    LOG_E(UTIL, "Error fcntl (%d:%s)\n", errno, strerror(errno));
    return err;
  }

  return fd;
}

int tun_init_mbms(char *ifname)
{
  nas_sock_mbms_fd = tuntap_alloc(IFF_TUN, ifname);

  if (nas_sock_mbms_fd == -1) {
    LOG_E(UTIL, "Error opening mbms socket %s (%d:%s)\n", ifname, errno, strerror(errno));
    exit(1);
  }

  LOG_D(UTIL, "Opened socket %s with fd %d\n", ifname, nas_sock_mbms_fd);

  struct sockaddr_nl nas_src_addr = {0};
  nas_src_addr.nl_family = AF_NETLINK;
  nas_src_addr.nl_pid = 1;
  nas_src_addr.nl_groups = 0; /* not in mcast groups */
  bind(nas_sock_mbms_fd, (struct sockaddr *)&nas_src_addr, sizeof(nas_src_addr));

  return 1;
}

int tun_init(const char *ifname, int instance_id)
{
  nas_sock_fd[instance_id] = tuntap_alloc(IFF_TUN, ifname);

  if (nas_sock_fd[instance_id] == -1) {
    LOG_E(UTIL, "Error opening socket %s (%d:%s)\n", ifname, errno, strerror(errno));
    return 0;
  }

  LOG_I(UTIL, "Opened socket %s with fd nas_sock_fd[%d]=%d\n", ifname, instance_id, nas_sock_fd[instance_id]);
  return 1;
}

/*
 * \brief set a genneric interface parameter
 * \param ifn the name of the interface to modify
 * \param if_addr the address that needs to be modified
 * \param operation one of SIOCSIFADDR (set interface address), SIOCSIFNETMASK
 *   (set network mask), SIOCSIFBRDADDR (set broadcast address), SIOCSIFFLAGS
 *   (set flags)
 * \return true on success, false otherwise
 */
static bool setInterfaceParameter(int sock_fd, const char *ifn, int af, const char *if_addr, int operation)
{
  DevAssert(af == AF_INET || af == AF_INET6);
  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name) - 1);
  struct in6_ifreq ifr6 = {0};

  void *ioctl_opt = NULL;
  if (af == AF_INET) {
    struct sockaddr_in addr = {.sin_family = AF_INET};
    int ret = inet_pton(af, if_addr, &addr.sin_addr);
    if (ret != 1) {
      LOG_E(OIP, "inet_pton(): cannot convert %s to IPv4 network address\n", if_addr);
      return false;
    }
    memcpy(&ifr.ifr_ifru.ifru_addr,&addr,sizeof(struct sockaddr_in));
    ioctl_opt = &ifr;
  } else {
    struct sockaddr_in6 addr6 = {.sin6_family = AF_INET6};
    int ret = inet_pton(af, if_addr, &addr6.sin6_addr);
    if (ret != 1) {
      LOG_E(OIP, "inet_pton(): cannot convert %s to IPv6 network address\n", if_addr);
      return false;
    }
    memcpy(&ifr6.ifr6_addr, &addr6.sin6_addr, sizeof(struct in6_addr));
    // we need to get the if index to put it into ifr6
    if (ioctl(sock_fd, SIOGIFINDEX, &ifr) < 0) {
      LOG_E(OIP, "ioctl() failed: errno %d, %s\n", errno, strerror(errno));
      return false;
    }
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 64;
    ioctl_opt = &ifr6;
  }

  bool success = ioctl(sock_fd, operation, ioctl_opt) == 0;
  if (!success)
    LOG_E(OIP, "Setting operation %d for %s: ioctl call failed: %d, %s\n", operation, ifn, errno, strerror(errno));
  return success;
}


/*
* \brief Get an interface's flags.
* \param[in]  sock_fd socket to use for getting the flags
* \param[in]  ifn the interface name for which to get flags
* \param[out] flags the flags of the interface
* \return true on success, false otherwise
*/
static bool get_if_flags(int sock_fd, const char *ifn, short *flags)
{
  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name) - 1);

  /* get flags of this interface: see netdevice(7) */
  int success = ioctl(sock_fd, SIOCGIFFLAGS, (caddr_t)&ifr) == 0;
  if (!success) {
    LOG_W(OIP, "On interface %s: ioctl() failed: %d, %s\n", ifn, errno, strerror(errno));
    return false;
  }
  *flags = ifr.ifr_flags;
  return true;
}

/*
 * \brief Set an interface's flags.
* \param[in]  sock_fd socket to use for setting the flags
* \param[in]  ifn the interface name for which to set flags
* \param[in]  flags the flags to set for the interface
* \return true on success, false otherwise
 */
static bool set_if_flags(int sock_fd, const char *ifn, short flags)
{
  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name) - 1);
  ifr.ifr_flags = flags;

  int success = ioctl(sock_fd, SIOCSIFFLAGS, (caddr_t)&ifr) == 0;
  if (!success) {
    LOG_W(OIP, "On interface %s: ioctl() failed: %d, %s\n", ifn, errno, strerror(errno));
    return false;
  }
  return true;
}

short tuntap_set_up(const char *ifname, int sock_fd)
{
  short flags = 0;
  if (!get_if_flags(sock_fd, ifname, &flags))
    return -1;

  flags |= IFF_UP;
  if (!set_if_flags(sock_fd, ifname, flags))
    return -1;

  return flags;
}

bool tun_config(const char* ifname, const char *ipv4, const char *ipv6)
{
  AssertFatal(ipv4 != NULL || ipv6 != NULL, "need to have IP address, but none given\n");

  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    LOG_E(UTIL, "Failed creating socket for interface management: %d, %s\n", errno, strerror(errno));
    return false;
  }

  bool success = true;
  if (ipv4 != NULL)
    success = setInterfaceParameter(sock_fd, ifname, AF_INET, ipv4, SIOCSIFADDR);
  // set the machine network mask for IPv4
  if (success && ipv4 != NULL)
    success = setInterfaceParameter(sock_fd, ifname, AF_INET, "255.255.255.0", SIOCSIFNETMASK);

  if (ipv6 != NULL) {
    // for setting the IPv6 address, we need an IPv6 socket. For setting IPv4,
    // we need an IPv4 socket. So do all operations using IPv4 socket, except
    // for setting the IPv6
    int sock_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
      LOG_E(UTIL, "Failed creating socket for interface management: %d, %s\n", errno, strerror(errno));
      success = false;
    }
    success = success && setInterfaceParameter(sock_fd, ifname, AF_INET6, ipv6, SIOCSIFADDR);
    close(sock_fd);
  }

  if (success) {
    const short flags = tuntap_set_up(ifname, sock_fd);
    success = flags >= 0 && set_if_flags(sock_fd, ifname, (flags | IFF_NOARP | IFF_POINTOPOINT) & ~IFF_MULTICAST);
  }

  if (success)
    LOG_A(OIP, "TUN Interface %s successfully configured, IPv4 %s, IPv6 %s\n", ifname, ipv4, ipv6);
  else
    LOG_E(OIP, "Interface %s couldn't be configured (IPv4 %s, IPv6 %s)\n", ifname, ipv4, ipv6);

  close(sock_fd);
  return success;
}

bool tap_config(const char* ifname)
{
  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    LOG_E(UTIL, "tap_config: failed creating socket %d, %s\n", errno, strerror(errno));
    return false;
  }

  const bool success = tuntap_set_up(ifname, sock_fd) >= 0;

  if (success)
    LOG_A(OIP, "TAP interface %s successfully configured\n", ifname);
  else
    LOG_E(OIP, "Tap interface %s couldn't be configured\n", ifname);

  close(sock_fd);
  return success;
}

void setup_ue_ipv4_route(const char* ifname, int instance_id, const char *ipv4)
{
  int table_id = instance_id - 1 + 10000;

  char command_line[500];
  int res = sprintf(command_line,
                    "ip rule add from %s/32 table %d && "
                    "ip rule add to %s/32 table %d && "
                    "ip route add default dev %s table %d",
                    ipv4,
                    table_id,
                    ipv4,
                    table_id,
                    ifname,
                    table_id);

  if (res < 0) {
    LOG_E(UTIL, "Could not create ip rule/route commands string\n");
    return;
  }
  background_system(command_line);
}

int tun_generate_ifname(char *ifname, const char *ifprefix, int instance_id)
{
  return snprintf(ifname, IFNAMSIZ, "%s%d", ifprefix, instance_id + 1);
}

int tuntap_generate_ue_ifname(char *ifname, int flag, int instance_id, int pdu_session_id)
{
  DevAssert(flag == IFF_TUN || flag == IFF_TAP);
  char pdu_session_string[10];
  snprintf(pdu_session_string, sizeof(pdu_session_string), "p%d", pdu_session_id);
  const char *basename = flag == IFF_TUN ? "oaitun_ue" : "oaitap_ue";
  // ifname: oaitun_ue<ue_id+1>[p<pdu_session_id>] when not default
  return snprintf(ifname, IFNAMSIZ, "%s%d%s", basename, instance_id + 1, pdu_session_id == -1 ? "" : pdu_session_string);
}

void tuntap_destroy(const char *dev)
{
  // Use a new socket for ioctl operations
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    LOG_E(UTIL, "Failed to create socket for interface teardown: %d, %s\n", errno, strerror(errno));
    return;
  }

  // interface not up => down
  short flags = 0;
  bool success = get_if_flags(fd, dev, &flags);
  success = success && set_if_flags(fd, dev, flags & ~IFF_UP);
  if (success) {
    LOG_I(UTIL, "Interface %s is now down.\n", dev);
  } // no else: get_if_flags()/set_if_flags() should have printed error
  close(fd);
}
