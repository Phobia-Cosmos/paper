// Shared memory layout for IPC demo with 256 pages of 4KB each
// Each page is represented as a data block with a ready flag for synchronization.
// This header is intended for use by both producer and consumer processes.

#ifndef IPC_SHARED_DEMO_SHM_LAYOUT_H
#define IPC_SHARED_DEMO_SHM_LAYOUT_H

#include <stdatomic.h>
#include <stdint.h>

// 256 blocks of 4KB data
#define IPC_NUM_BLOCKS 256
#define IPC_BLOCK_SIZE 4096

typedef struct {
  _Atomic uint8_t ready; // 0 = empty, 1 = full/ready for read
  uint8_t data[IPC_BLOCK_SIZE];
} ipc_block_t;

typedef struct {
  _Atomic uint32_t write_index; // index of next block to write
  _Atomic uint32_t read_index;  // index of next block to read
  ipc_block_t blocks[IPC_NUM_BLOCKS];
} ipc_shared_t;

#endif // IPC_SHARED_DEMO_SHM_LAYOUT_H
