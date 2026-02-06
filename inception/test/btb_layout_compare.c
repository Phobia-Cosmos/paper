#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

/*
 * 注意：
 * - section 名字始终存在
 * - 是否分页，完全取决于 linker 是否用 link.ld
 */

__attribute__((noinline, section(".text.a"))) void target_a(void)
{
    asm volatile(
        ".rept 1500\n\t"
        "nop\n\t"
        ".endr\n\t" ::: "memory");
}

__attribute__((noinline, section(".text.b"))) void target_b(void)
{
    asm volatile(
        ".rept 1500\n\t"
        "nop\n\t"
        ".endr\n\t" ::: "memory");
}

/* ---------------- TSC helpers ---------------- */

static inline uint64_t rdtsc_begin(void)
{
    uint64_t lo, hi;
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "%rbx", "%rcx");
    return (hi << 32) | lo;
}

static inline uint64_t rdtsc_end(void)
{
    uint64_t lo, hi;
    asm volatile(
        "rdtscp\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "%rbx", "%rcx");
    asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

void flush(void *p)
{
    asm volatile("clflush 0(%0)\n"
                 :
                 : "c"(p)
                 : "rax");
}

/* ---------------- Experiment ---------------- */

int main(void)
{
    void (*fn)(void), (*fna)(void), (*fnb)(void);
    uint64_t t1, t2;

    printf("target_a @ %p\n", target_a);
    printf("target_b @ %p\n", target_b);
    printf("page diff = %ld pages\n",
           ((uintptr_t)target_b >> 12) -
               ((uintptr_t)target_a >> 12));

    /* 冷却前端状态 */
    asm volatile(".rept 200000\n\tnop\n\t.endr\n\t");
    fna = target_a;
    fnb = target_b;
    // fna();
    // fnb();

    /* Phase 1: BTB training */
    fn = target_a;
    for (int i = 0; i < 200000; i++)
        fn();
    flush(fna);
    flush(fnb);

    /* Phase 2: BTB hit */
    t1 = rdtsc_begin();
    fn();
    t2 = rdtsc_end();
    printf("[BTB HIT ] cycles = %lu\n", t2 - t1);

    asm volatile(".rept 200000\n\tnop\n\t.endr\n\t");
    flush(fna);
    flush(fnb);

    /* Phase 3: BTB miss */
    fn = target_b;

    t1 = rdtsc_begin();
    fn();
    t2 = rdtsc_end();
    printf("[BTB MISS] cycles = %lu\n", t2 - t1);

    return 0;
}
