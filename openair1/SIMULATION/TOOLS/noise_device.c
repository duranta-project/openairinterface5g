/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "assertions.h"
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>
#include "noise_device.h"
#include <simde/x86/avx.h>

// Use block size equal to cache size
#define BLOCK_SIZE (64 / sizeof(float)) // 4 bytes per float
// Use a power of 2 for block size length
#define NUM_BLOCKS (8192)
#define NOISE_VECTOR_LENGTH (BLOCK_SIZE * NUM_BLOCKS)

struct noise_device_s {
  float noise[NOISE_VECTOR_LENGTH] __attribute__((aligned(64)));
  float noise_power;
  pthread_t thread;
  pthread_mutex_t lock;
  pthread_cond_t stop_cond;
  bool running; // protected by lock
  // Picks the start block of each read; shared by every thread reading from the device.
  _Atomic uint32_t counter;
};

// Random start block, safe to call from several threads at once.
static int random_block(noise_device_t *dev)
{
  uint32_t x = atomic_fetch_add_explicit(&dev->counter, 0x9e3779b9u, memory_order_relaxed);
  x ^= x >> 16;
  x *= 0x85ebca6bu;
  x ^= x >> 13;
  return x % NUM_BLOCKS;
}

static void generate_one_block_noise(noise_device_t *dev, int write_block_index)
{
  float block[BLOCK_SIZE];
  for (int i = 0; i < BLOCK_SIZE; i++) {
    block[i] = dev->noise_power * gaussZiggurat(0, 1);
  }
  memcpy(dev->noise + write_block_index * BLOCK_SIZE, block, sizeof(block));
}

// Keeps rewriting a random block of the table every 10 ms while readers use it.
static void *noise_device_thread_function(void *arg)
{
  noise_device_t *dev = arg;
  pthread_mutex_lock(&dev->lock);
  while (dev->running) {
    pthread_mutex_unlock(&dev->lock);
    generate_one_block_noise(dev, random_block(dev));
    pthread_mutex_lock(&dev->lock);
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += 10 * 1000 * 1000;
    if (t.tv_nsec >= 1000 * 1000 * 1000) {
      t.tv_sec++;
      t.tv_nsec -= 1000 * 1000 * 1000;
    }
    if (dev->running)
      pthread_cond_timedwait(&dev->stop_cond, &dev->lock, &t);
  }
  pthread_mutex_unlock(&dev->lock);
  return NULL;
}

noise_device_t *init_noise_device(float noise_power)
{
  noise_device_t *dev = aligned_alloc(64, (sizeof(*dev) + 63) / 64 * 64);
  AssertFatal(dev, "out of memory\n");
  dev->noise_power = noise_power;
  dev->counter = 123456789;
  for (int i = 0; i < NUM_BLOCKS; i++) {
    generate_one_block_noise(dev, i);
  }
  pthread_mutex_init(&dev->lock, NULL);
  pthread_cond_init(&dev->stop_cond, NULL);
  dev->running = true;
  int ret = pthread_create(&dev->thread, NULL, noise_device_thread_function, dev);
  AssertFatal(ret == 0, "pthread_create failed: %d, errno %d (%s)\n", ret, errno, strerror(errno));
  return dev;
}

void free_noise_device(noise_device_t *dev)
{
  pthread_mutex_lock(&dev->lock);
  dev->running = false;
  pthread_cond_signal(&dev->stop_cond);
  pthread_mutex_unlock(&dev->lock);
  int ret = pthread_join(dev->thread, NULL);
  AssertFatal(ret == 0, "pthread_join failed: %d, errno %d (%s)\n", ret, errno, strerror(errno));
  pthread_cond_destroy(&dev->stop_cond);
  pthread_mutex_destroy(&dev->lock);
  free(dev);
}

void get_noise_vector(noise_device_t *dev, float *noise_vector, int length)
{
  int start_block = random_block(dev);
  while (length > 0) {
    int copy_size = min(length, (NUM_BLOCKS - start_block) * BLOCK_SIZE);
    memcpy(noise_vector, &dev->noise[start_block * BLOCK_SIZE], copy_size * sizeof(float));
    start_block = 0;
    length -= copy_size;
    noise_vector += copy_size;
  }
}

// vector[0 .. n) += noise[0 .. n): load, add, store in one pass (8 floats per simde vector; native
// AVX on x86, NEON pairs on aarch64).
static void add_noise_block(float *restrict vector, const float *restrict noise, int n)
{
  int i = 0;
  for (; i + 16 <= n; i += 16) {
    simde__m256 a = simde_mm256_add_ps(simde_mm256_loadu_ps(vector + i), simde_mm256_loadu_ps(noise + i));
    simde__m256 b = simde_mm256_add_ps(simde_mm256_loadu_ps(vector + i + 8), simde_mm256_loadu_ps(noise + i + 8));
    simde_mm256_storeu_ps(vector + i, a);
    simde_mm256_storeu_ps(vector + i + 8, b);
  }
  for (; i + 8 <= n; i += 8)
    simde_mm256_storeu_ps(vector + i, simde_mm256_add_ps(simde_mm256_loadu_ps(vector + i), simde_mm256_loadu_ps(noise + i)));
  for (; i < n; i++)
    vector[i] += noise[i];
}

void add_noise_vector(noise_device_t *dev, float *vector, int length)
{
  int start_block = random_block(dev);
  while (length > 0) {
    int add_size = min(length, (NUM_BLOCKS - start_block) * BLOCK_SIZE);
    add_noise_block(vector, &dev->noise[start_block * BLOCK_SIZE], add_size);
    start_block = 0;
    length -= add_size;
    vector += add_size;
  }
}
