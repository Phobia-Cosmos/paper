#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

/* -------------------------------------------------
 * Force target_a and target_b into different sections
 * ------------------------------------------------- */

__attribute__((noinline, section(".text.a"))) void target_a(void)
{
    asm volatile(
        ".rept 1280\n\t"
        "nop\n\t"
        ".endr\n\t" ::: "memory");
}

__attribute__((noinline, section(".text.b"))) void target_b(void)
{
    asm volatile(
        ".rept 1280\n\t"
        "nop\n\t"
        ".endr\n\t" ::: "memory");
}

/* -------------------------------------------------
 * TSC helpers
 * ------------------------------------------------- */

static inline __attribute__((always_inline))
uint64_t
rdtsc_begin(void)
{
    uint64_t lo, hi;
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        : "=r"(hi), "=r"(lo)
        :
        : "%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

static inline __attribute__((always_inline))
uint64_t
rdtsc_end(void)
{
    uint64_t lo, hi;
    asm volatile(
        "rdtscp\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        "cpuid\n\t"
        : "=r"(hi), "=r"(lo)
        :
        : "%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

int main(void)
{
    void (*fn)(void);
    uint64_t t1, t2;

    /* Front-end & BTB cooldown */
    asm volatile(
        ".rept 200000\n\t"
        "nop\n\t"
        ".endr\n\t");

    /* -----------------------------
     * Phase 1: Train BTB
     * ----------------------------- */
    fn = target_a;
    for (int i = 0; i < 200000; i++)
    {
        fn();
    }

    /* -----------------------------
     * Phase 2: BTB hit
     * ----------------------------- */
    t1 = rdtsc_begin();
    fn();
    t2 = rdtsc_end();
    printf("[BTB HIT ] cycles = %lu\n", t2 - t1);

    /* Cooldown */
    asm volatile(
        ".rept 200000\n\t"
        "nop\n\t"
        ".endr\n\t");

    /* -----------------------------
     * Phase 3: BTB mispredict
     * ----------------------------- */
    fn = target_b;

    t1 = rdtsc_begin();
    fn();
    t2 = rdtsc_end();
    printf("[BTB MISS] cycles = %lu\n", t2 - t1);

    return 0;
}
