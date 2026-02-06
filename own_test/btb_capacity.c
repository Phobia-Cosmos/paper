#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <sched.h>
#include <unistd.h>

volatile int sink;

/* ========== rdtsc / rdtscp ========== */

static inline __attribute__((always_inline)) uint64_t rdtsc(void)
{
    uint64_t lo, hi;
    asm volatile("CPUID\n\t"
                 "RDTSC\n\t"
                 "movq %%rdx, %0\n\t"
                 "movq %%rax, %1\n\t"
                 : "=r"(hi), "=r"(lo)
                 :
                 : "%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

static inline __attribute__((always_inline)) uint64_t rdtscp(void)
{
    uint64_t lo, hi;
    asm volatile("RDTSCP\n\t"
                 "movq %%rdx, %0\n\t"
                 "movq %%rax, %1\n\t"
                 "CPUID\n\t"
                 : "=r"(hi), "=r"(lo)
                 :
                 : "%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

/* ========== target ========== */

__attribute__((noinline)) void target(void)
{
    sink++;
}

/* ========== branches ========== */

#include "gen_branches.h"

typedef void (*branch_fn)(void (*)(void));

branch_fn branches[1024 * 2] = {
#define X(i) branch_##i,
#include "branch_list.h"
#undef X
};

/* ========== main experiment ========== */

int main(void)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);

    const int TRAIN = 1000;

    printf("#branches latency(cycles)\n");

    for (int n = 1; n <= 1024 * 2; n++)
    {

        /* BTB training */
        for (int r = 0; r < TRAIN; r++)
        {
            for (int i = 0; i < n; i++)
            {
                branches[i](target);
            }
        }

        /* measure branch[0] */
        uint64_t t0 = rdtsc();
        branches[0](target);
        uint64_t t1 = rdtscp();

        printf("%4d %6lu\n", n, t1 - t0);
    }

    for (int i = 0; i < 10; i++)
        printf("%p\n", branches[i]);
}
