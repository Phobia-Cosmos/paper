// Safe, educational side-channel demonstration (single-process)
// Demonstrates how a victim thread can affect cache timing for an attacker thread
// purely for defensive understanding. This is not a real cross-process flush-reload attack.
// Build:  gcc -pthread defense_demo/demo_sidechannel.c -O2 -Wall -o defense_demo/demo_sidechannel

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

typedef struct {
    atomic_uint secret_bit;      // 0 or 1 chosen by victim
    volatile uint8_t probe[2];   // memory locations probed by attacker
} shared_t;

static shared_t shared;
static volatile int run = 1;

static inline uint64_t nanos_diff(struct timespec a, struct timespec b) {
    return (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000ULL + (uint64_t)(b.tv_nsec - a.tv_nsec);
}

void *victim_thread(void *arg) {
    (void)arg;
    // seed
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)pthread_self());
    // Initialize probe values to non-zero data
    shared.probe[0] = 0xAA;
    shared.probe[1] = 0x55;

    for (int i = 0; i < 60 && run; i++) {
        // Choose a secret bit and publish it
        unsigned bit = rand() & 1;
        atomic_store(&shared.secret_bit, bit);
        // Touch the corresponding probe to "cache" it for a moment
        volatile uint8_t dummy = shared.probe[bit];
        (void)dummy;
        // Yield to attacker to observe timing
        sched_yield();
        // Short delay to reduce tight loop effects
        usleep(1000);
    }
    return NULL;
}

uint64_t time_read(uint8_t *addr) {
    struct timespec ts1, ts2;
    volatile uint8_t tmp = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    tmp = *addr;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    (void)tmp;
    return nanos_diff(ts1, ts2);
}

void *attacker_thread(void *arg) {
    (void)arg;
    uint64_t t0, t1;
    int correct = 0;
    int total = 0;
    for (int i = 0; i < 60; i++) {
        // Measure access time for both probes
        t0 = time_read((uint8_t *)&shared.probe[0]);
        t1 = time_read((uint8_t *)&shared.probe[1]);
        // Infer which location was more recently cached by victim
        int guess = (t0 < t1) ? 0 : 1;
        unsigned secret = atomic_load(&shared.secret_bit);
        if (guess == (int)secret) correct++;
        total++;
        printf("[attacker] iter=%d guess=%d secret=%u times=%lu/%lu -> %s (acc=%d/%d)\n",
               i, guess, secret, (unsigned long)t0, (unsigned long)t1,
               (guess == (int)secret) ? "hit" : "miss",
               correct, total);
        // brief pause
        usleep(20000);
    }
    run = 0; // signal end
    return NULL;
}

int main(void) {
    pthread_t t_victim, t_attacker;
    shared.secret_bit = 0;
    // Create threads
    if (pthread_create(&t_victim, NULL, victim_thread, NULL) != 0) {
        perror("pthread_create victim");
        return 1;
    }
    if (pthread_create(&t_attacker, NULL, attacker_thread, NULL) != 0) {
        perror("pthread_create attacker");
        return 1;
    }
    // Join threads
    pthread_join(t_victim, NULL);
    pthread_join(t_attacker, NULL);
    printf("[defense_demo] complete. Attacker results shown above.\n");
    return 0;
}
