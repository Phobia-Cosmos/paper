#pragma once
#include <stdint.h>
#include <x86intrin.h>

#define PAGE_SIZE 4096
#define PROBE_PAGES 256
#define CACHE_HIT_THRESHOLD 80

static inline uint64_t rdtsc_begin(void)
{
    uint64_t lo, hi;
    asm volatile(
        "CPUID\n\t"
        "RDTSC\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        : "=r"(hi), "=r"(lo)::"%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

static inline uint64_t rdtsc_end(void)
{
    uint64_t lo, hi;
    asm volatile(
        "RDTSCP\n\t"
        "mov %%rdx, %0\n\t"
        "mov %%rax, %1\n\t"
        "CPUID\n\t"
        : "=r"(hi), "=r"(lo)::"%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}
