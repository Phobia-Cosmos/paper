#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define SIZE (2*1024*1024)

int main() {
    void *addr;
    int fd_pagemap = open("/proc/self/pagemap", O_RDONLY);

    // 尝试多次分配找对齐地址
    for (int i = 0; i < 100; i++) {
        addr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

        if (addr == MAP_FAILED) {
            perror("mmap");
            continue;
        }

        // 检查物理地址
        unsigned long va = (unsigned long)addr;
        unsigned long offset = va >> 12;
        unsigned long pfn;

        lseek(fd_pagemap, offset * 8, SEEK_SET);
        if (read(fd_pagemap, &pfn, 8) != 8) {
            munmap(addr, SIZE);
            continue;
        }

        pfn = pfn & 0x7FFFFFFFFFFFFF;
        unsigned long pa = pfn << 12;

        printf("Try %d: va=%lx, pa=%lx, aligned=%d\n", i, va, pa, (pa & 0x1fffff) == 0);

        if ((pa & 0x1fffff) == 0) {
            printf("Found aligned! pa=%lx\n", pa);
            munmap(addr, SIZE);
            break;
        }

        munmap(addr, SIZE);
    }

    close(fd_pagemap);
    return 0;
}
