// Producer for IPC shared memory demo
// Creates/uses a POSIX shared memory object and writes 4KB blocks into a 256-block ring.
// Run first in one terminal: ./producer
// Run a second terminal: ./consumer

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>

#include "shm_layout.h"

static const char *SHM_NAME = "/ipc_shared_demo_256";
static const size_t SHM_SIZE = sizeof(ipc_shared_t);

static ipc_shared_t *shared = NULL;

static void die(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

int main(void)
{
  // Create or open shared memory object
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd < 0)
    die("shm_open");
  if (ftruncate(fd, SHM_SIZE) != 0)
    die("ftruncate");
  void *ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED)
    die("mmap");
  shared = (ipc_shared_t *)ptr;

  // Initialize shared state if first creation
  // Best-effort: reset indices and block flags
  atomic_store(&shared->write_index, 0);
  atomic_store(&shared->read_index, 0);
  for (int i = 0; i < IPC_NUM_BLOCKS; i++)
  {
    atomic_store(&shared->blocks[i].ready, 0);
  }

  // Produce a number of blocks
  for (int i = 0; i < 200; i++)
  {
    // simple backpressure: wait if next block not consumed
    while ((atomic_load(&shared->write_index) - atomic_load(&shared->read_index)) >= IPC_NUM_BLOCKS)
    {
      sched_yield();
    }
    uint32_t idx = atomic_fetch_add(&shared->write_index, 1) & (IPC_NUM_BLOCKS - 1);
    // Fill block data
    for (size_t j = 0; j < IPC_BLOCK_SIZE; j++)
    {
      shared->blocks[idx].data[j] = (uint8_t)(i + j);
    }
    // Mark ready
    atomic_store(&shared->blocks[idx].ready, 1);
    // small pause to simulate work (5ms)
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 5L * 1000L * 1000L; // 5ms
    nanosleep(&ts, NULL);
  }

  printf("Producer done. Exiting.\n");
  // Optional cleanup: keep memory for consumer to read; comment if you want cleanup
  // shm_unlink(SHM_NAME);
  return 0;
}
