IPC Shared Memory Demo

Overview:
- Demonstrates a simple producer/consumer IPC using a single shared memory region.
- The region is organized as 256 blocks of 4KB each with ready flags for simple synchronization.
- This is intended for defensive understanding of IPC memory sharing and does not implement any side-channel attacks.

Build:
- go to defense_demo/ipc_shared_demo
- make

Usage:
- Run producer in one terminal: ./producer
- Run consumer in another terminal: ./consumer
- Optional: call shm_unlink after run to clean up (producer can do it).
