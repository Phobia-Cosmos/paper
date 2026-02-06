#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "shared.h"

const char *secret = "HELLO_FLUSH_RELOAD";

int main(void)
{
    int fd = open(SHARED_FILE, O_RDWR | O_CREAT, 0666);
    ftruncate(fd, SHARED_SIZE);

    uint8_t *probe = mmap(NULL,
                          SHARED_SIZE,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fd, 0);

    close(fd);

    memset(probe, 1, SHARED_SIZE);

    printf("[Victim] PID=%d\n", getpid());
    fflush(stdout);

    for (size_t i = 0; i < strlen(secret); i++)
    {
        uint8_t value = secret[i];
        volatile uint8_t *addr = probe + value * PAGE_SIZE;
        *addr += 1; // 真正触发共享 cache
        usleep(1000);
    }

    sleep(1);
    return 0;
}
