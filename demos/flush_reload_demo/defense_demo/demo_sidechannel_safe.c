// Safe, educational side-channel demonstration (single-process)
// In-process victim-attacker using a shared page to illustrate cache timing effects.
// This is a defensive teaching tool: no real cross-process exfiltration.
// Build:  gcc -pthread defense_demo/demo_sidechannel_safe.c -O2 -Wall -o defense_demo/demo_sidechannel_safe

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#ifndef PAGE_SHIFT
#define PAGE_SIZE 4096
#else
#define PAGE_SIZE (1 << PAGE_SHIFT)
#endif

// A single shared page (aligned to a page boundary for realism in other contexts)
static uint8_t shared_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static atomic_uint secret_bit;
static volatile int run = 1;

static inline uint64_t nanos_diff(struct timespec a, struct timespec b) {
    return (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000ULL + (uint64_t)(b.tv_nsec - a.tv_nsec);
}

// Time to read a given address
static uint64_t time_read(uint8_t *addr) {
    struct timespec t1, t2;
    volatile uint8_t tmp = 0;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    tmp = *addr;
    clock_gettime(CLOCK_MONOTONIC, &t2);
    (void)tmp;
    return nanos_diff(t1, t2);
}

// Victim: writes a secret bit and touches a cache line related to that bit
void *victim_thread(void *arg) {
    (void)arg;
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)pthread_self());
    // Initialize page with distinct values
    shared_page[0] = 0xAA;
    shared_page[PAGE_SIZE/2] = 0xBB;

    for (int i = 0; i < 120 && run; i++) {
        unsigned bit = rand() & 1;
        atomic_store(&secret_bit, bit);
        // Access the corresponding cache line to emulate a cache-hot location
        volatile uint8_t v = shared_page[(bit ? (PAGE_SIZE/2) : 0)];
        (void)v;
        // Optional mitigation path: touch the other line too to try to neutralize timing
        #ifdef MITIGATION
        volatile uint8_t w = shared_page[(bit ? 0 : (PAGE_SIZE/2))];
        (void)w;
        #endif
        sched_yield();
        usleep(1000);
    }
    return NULL;
}

void *attacker_thread(void *arg) {
    (void)arg;
    unsigned hits = 0;
    unsigned total = 0;
    for (int i = 0; i < 120; i++) {
        uint64_t t0 = time_read(&shared_page[0]);
        uint64_t t1 = time_read(&shared_page[PAGE_SIZE/2]);
        int guess = (t0 < t1) ? 0 : 1;
        unsigned secret = atomic_load(&secret_bit);
        if (guess == (int)secret) hits++;
        total++;
        printf("[attacker] iter=%d guess=%d secret=%u t0=%lu t1=%lu -> %s (%%hit=%u/%u)\n",
               i, guess, secret, (unsigned long)t0, (unsigned long)t1,
               (guess == (int)secret) ? "hit" : "miss", hits, total);
        usleep(20000);
    }
    run = 0;
    return NULL;
}

int main(void) {
    pthread_t t_victim, t_attacker;
    atomic_init(&secret_bit, 0);
    // Create both threads
    if (pthread_create(&t_victim, NULL, victim_thread, NULL) != 0) {
        perror("pthread_create victim");
        return 1;
    }
    if (pthread_create(&t_attacker, NULL, attacker_thread, NULL) != 0) {
        perror("pthread_create attacker");
        return 1;
    }
    // Join
    pthread_join(t_victim, NULL);
    pthread_join(t_attacker, NULL);
    printf("[safe_demo] completed.\n");
    return 0;
}
