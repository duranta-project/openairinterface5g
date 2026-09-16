/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_shm_region.c
 * @brief Shared POSIX-shm lifecycle (see e3_shm_region.h).
 */
#include "e3_shm_region.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/utils/LOG/log.h"

int e3_shm_region_create(e3_shm_region_t *region, const e3_shm_region_desc_t *desc)
{
  if (!region || !desc || !desc->name || desc->size == 0)
    return -1;

  /* Drop any stale segment from a previous crash. Single-tenant per host. */
  shm_unlink(desc->name);

  int fd = shm_open(desc->name, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    LOG_E(E3AP, "[%s] shm_open(%s) failed: %s\n", desc->log_tag, desc->name, strerror(errno));
    return -1;
  }

  /* fchmod defeats umask so a dApp in another container can mmap O_RDONLY. */
  if (fchmod(fd, 0644) < 0) {
    LOG_W(E3AP, "[%s] fchmod 0644 failed: %s (continuing)\n", desc->log_tag, strerror(errno));
  }

  if (ftruncate(fd, (off_t)desc->size) < 0) {
    LOG_E(E3AP, "[%s] ftruncate(%zu) failed: %s\n", desc->log_tag, desc->size, strerror(errno));
    goto fail_fd;
  }

  void *base = mmap(NULL, desc->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (base == MAP_FAILED) {
    LOG_E(E3AP, "[%s] mmap failed: %s\n", desc->log_tag, strerror(errno));
    goto fail_fd;
  }

  region->fd = fd;
  region->base = base;
  region->size = desc->size;
  region->desc = desc;
  return 0;

fail_fd:
  close(fd);
  shm_unlink(desc->name);
  return -1;
}

void e3_shm_region_destroy(e3_shm_region_t *region)
{
  if (!region)
    return;
  if (region->base && region->base != MAP_FAILED)
    munmap(region->base, region->size);
  if (region->fd >= 0 && region->desc) {
    close(region->fd);
    shm_unlink(region->desc->name);
  }
  region->fd = -1;
  region->base = NULL;
  region->size = 0;
}
