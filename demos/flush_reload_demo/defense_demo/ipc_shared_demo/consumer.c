// Consumer for IPC shared memory demo
// Maps the same shared memory object and reads 4KB blocks

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
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

static void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int main(void) {
  int fd = shm_open(SHM_NAME, O_RDONLY, 0666);
  if (fd < 0) {
    perror("shm_open consumer");
    return EXIT_FAILURE;
  }
  void *ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    perror("mmap consumer");
    return EXIT_FAILURE;
  }
  shared = (ipc_shared_t *)ptr;

  // Read a fixed number of blocks
  for (int i = 0; i < 200; i++) {
    uint32_t ridx;
    // Busy-wait for a block to become ready
    uint32_t read_count = atomic_load(&shared->read_index);
    while (1) {
      uint32_t idx = read_count & (IPC_NUM_BLOCKS - 1);
      if (atomic_load(&shared->blocks[idx].ready) == 1) {
        // Read some data
        volatile uint8_t sample = shared->blocks[idx].data[0];
        (void)sample;
        // Mark as consumed
        atomic_store(&shared->blocks[idx].ready, 0);
        atomic_fetch_add(&shared->read_index, 1);
        break;
      } else {
        // Sleep briefly to avoid busy spinning too aggressively
        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 1000000; // 1ms
        nanosleep(&ts, NULL);
      }
      read_count = atomic_load(&shared->read_index);
    }
  }
  printf("Consumer done.\n");
  return 0;
}
