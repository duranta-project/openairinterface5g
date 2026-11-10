/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "pl2303.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include "log.h"

void init_card(card_t *card, char *card_filename)
{
  /* same config as found in Laurent Thomas uicc program (except VMIN and
   * VTIME that are problematic, leading to failures, for unknown reason)
   */
  int fd = open(card_filename, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd == -1) { error("%s: %s\n", card_filename, strerror(errno)); exit(1); }
  struct termios tty;
  if (tcgetattr(fd, &tty) < 0) { error("tcgetattr() failed: %s\n", strerror(errno)); exit(1); }
  tty.c_cflag &= ~(CSIZE);
  tty.c_cflag |= CLOCAL | CREAD | CS8 | PARENB | CSTOPB | HUPCL;
  /* setup for non-canonical mode */
  tty.c_iflag &= ~(IGNBRK | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;
  cfsetispeed(&tty, (speed_t)B9600);
  cfsetospeed(&tty, (speed_t)B9600);
  if (tcsetattr(fd, TCSANOW, &tty)) { error("tcsetattr() failed: %s\n", strerror(errno)); exit(1); }

  card->fd = fd;
  card->name = strdup(card_filename); if (!card->name) { error("out of memory\n"); exit(1); }
}

void close_card(card_t *card)
{
  close(card->fd);
  free(card->name);
  card->fd = -1;
  card->name = 0;
}

void reset_card(card_t *card)
{
  int fd = card->fd;

  /* activate RST (that is: set the line LOW, which is done with TIOCMBIS) */
  int bits = TIOCM_RTS; //on the device of Laurent RST is controlled by TIOCM_RTS
  int method;
  method = TIOCMBIS;
  if (ioctl(fd, method, &bits) < 0) { error("ioctl() failed: %s\n", strerror(errno)); exit(1); }
  /* wait 400 mus minimum, see ISO 7816-3 6.2.3 warm reset, let's say 1ms */
  usleep(1000);
  /* decativate RST (so set line to HIGH, done with TIOCMBIC) */
  method = TIOCMBIC;
  bits = TIOCM_RTS;
  if (ioctl(fd, method, &bits) < 0) { error("ioctl() failed: %s\n", strerror(errno)); exit(1); }
  /* wait a bit (1ms say) */
  usleep(1000);
}

unsigned char get_byte(card_t *card, int timeout_ms)
{
  struct pollfd fds[1];
  fds[0].fd = card->fd;
  fds[0].events = POLLIN;
  int r = poll(fds, 1, timeout_ms);
  if (r == -1) { error("poll() failed: %s\n", strerror(errno)); exit(1); }
  if (r == 0) { error("get_byte() timeout\n"); exit(1); }
  if (fds[0].revents != POLLIN) { error("poll revents is bad\n"); exit(1); }
  char ret;
  r = read(card->fd, &ret, 1);
  if (r < 0) { error("read() failed: %s\n", strerror(errno)); exit(1); }
  if (r != 1) { error("read() failed in get_byte()\n"); exit(1); }
  debug("get_byte(): got %2.2x\n", (unsigned char)ret);
  return ret;
}

void put_bytes(card_t *card, unsigned char *bytes, int count)
{
  debug("put_bytes");
  for (int i = 0; i < count; i++) debug(" %2.2x", bytes[i]);
  debug("\n");
  /* to be refined if needed (write() may return < count) */
  if (write(card->fd, bytes, count) != count) { error("put_bytes failed\n"); exit(1); }

  /* bytes are echoed back, swallow them */
  char x[count];
  int ret;
  int to_receive = count;
  while (to_receive) {
    ret = read(card->fd, x, to_receive);
    if (ret <= 0) { error("get_byte failed\n"); exit(1); }
    to_receive -= ret;
  }
}
