#include <err.h>
#include <string.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// 固定虚拟地址
#define RB_PTR 0x13370000
#define RB_STRIDE_BITS 12
#define RB_STRIDE (0x1UL << RB_STRIDE_BITS)
// 32 + 1（溢出位）
#define RB_SLOTS 0x21
#define RSB_SIZE 0x20

#if defined(ZEN)
#define PTRN 0x20100000000UL // Zen(+)
#else
#define PTRN 0x100100000000UL // Zen 2
#endif

#define ROUNDS 1000

// 内存映射行为语义:不映射任何文件,物理页来自：buddy allocator,只属于当前进程,这是一段 纯内存缓冲区，不是 mmap 文件
#define MMAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED_NOREPLACE)
// 页级访问权限（PTE permissions），由内核写入页表;可读 + 可写 + 不可执行 (RW-) 的用户态内存;防止 CPU 把 probe buffer 当作代码执行
#define PROT_RW (PROT_READ | PROT_WRITE)

__attribute__((aligned(4096))) static uint64_t results1[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results2[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results3[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results4[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results5[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results6[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results7[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results8[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results9[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results10[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results11[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results12[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results13[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results14[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results15[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results16[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results17[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results18[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results19[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results20[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results21[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results22[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results23[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results24[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results25[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results26[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results27[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results28[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results29[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results30[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results31[RB_SLOTS] = {0};
__attribute__((aligned(4096))) static uint64_t results32[RB_SLOTS] = {0};

#define NOPS_str(n) ".rept " xstr(n) "\n\t"    \
                                     "nop\n\t" \
                                     ".endr\n\t"

#define str(s) #s
#define xstr(s) str(s)

// CPUID + RDTSC;严格序列化
// 完全序列化（full serialization barrier）:之前的所有指令已经退休（retire）;pipeline 被清空;前端 / 后端都同步
// 读取时间戳计数器：EDX:EAX = TSC;可以被乱序执行;%eax：低 32 位;%edx：高 32 位
// 如果不执行CPUID，那么movq可能会提前或者延迟执行
// movq %rdx, hi
// movq %rax, lo
// CPUID 会修改 EAX/EBX/ECX/EDX 全部:RDTSCP 返回 TSC_AUX(rcx);破坏rbx
static inline __attribute__((always_inline)) uint64_t rdtsc(void)
{
    uint64_t lo, hi;
    asm volatile("CPUID\n\t"
                 "RDTSC\n\t"
                 "movq %%rdx, %0\n\t"
                 "movq %%rax, %1\n\t" : "=r"(hi), "=r"(lo)::"%rax", "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

// 读取 TSC;保证：之前的指令已经执行完成;不保证之后的指令不被提前执行
static inline __attribute__((always_inline)) uint64_t rdtscp(void)
{
    uint64_t lo, hi;
    asm volatile("RDTSCP\n\t"
                 "movq %%rdx, %0\n\t"
                 "movq %%rax, %1\n\t"
                 "CPUID\n\t" : "=r"(hi), "=r"(lo)::"%rax",
                               "%rbx", "%rcx", "%rdx");
    return (hi << 32) | lo;
}

// speculative execution 若访问了某一页，reload 时变成 cache hit
static inline __attribute__((always_inline)) void reload_range(long base, long stride, int n, uint64_t *results)
{
    // TODO：为什么要限制性这两个指令？为什么要有两个循环？为什么循环中c的地址是如此计算的，是什么意思？为什么循环只有长度n的一半？为什么每一次步长为2？为什么最后n为奇数时需要多测一次？
    asm("lfence");
    asm("mfence");
    int done = 0;
    for (volatile int k = 0; k < n / 2; k += 2)
    {
        uint64_t c = (n / 2) - 1 - ((k * 13 + 9) & ((n / 2) - 1));
        unsigned volatile char *p = (uint8_t *)base + (stride * c);
        uint64_t t0 = rdtsc();
        *(volatile unsigned char *)p;
        uint64_t dt = rdtscp() - t0;
        if (dt < 200)
            results[c]++;
        if (k == (n / 2) - 2 && !done)
        {
            k = -1;
            done = 1;
        }
    }
    asm("lfence");
    asm("mfence");

    done = 0;
    for (volatile int k = 0; k < n / 2; k += 2)
    {
        uint64_t c = (n / 2) + (n / 2) - 1 - ((k * 13 + 9) & ((n / 2) - 1));
        unsigned volatile char *p = (uint8_t *)base + (stride * c);
        uint64_t t0 = rdtsc();
        *(volatile unsigned char *)p;
        uint64_t dt = rdtscp() - t0;
        if (dt < 200)
            results[c]++;
        if (k == (n / 2) - 2 && !done)
        {
            k = -1;
            done = 1;
        }
    }
    asm("lfence");
    asm("mfence");

    if (n % 2 == 1)
    {
        unsigned volatile char *p = (uint8_t *)base + (stride * (n - 1));
        uint64_t t0 = rdtsc();
        *(volatile unsigned char *)p;
        uint64_t dt = rdtscp() - t0;
        if (dt < 200)
            results[n - 1]++;
    }
}

// TODO:传入的参数分别有和作用？
// Reload buffer 基址（RB_PTR）
//
static inline __attribute__((always_inline)) void flush_range(long start, long stride, int n)
{
    asm("lfence");
    asm("mfence");
    for (uint64_t k = 0; k < n; ++k)
    {
        volatile void *p = (uint8_t *)start + k * stride;
        __asm__ volatile("clflushopt (%0)\n" ::"r"(p));
        __asm__ volatile("clflushopt (%0)\n" ::"r"(p));
    }
    asm("lfence");
    asm("mfence");
}

#if defined(BTB)
void b();
void c();
uint64_t b_addr = (uint64_t)b;
uint64_t c_addr = (uint64_t)c;
#elif defined(PHT)
volatile uint64_t value[100] = {0};
#endif

int main(int argc, char *argv[])
{
    // 34 个 4KB 页,[0x13370000, 0x13370000 + 0x22000)
    // slot_i_addr = RB_PTR + i * (1 << RB_STRIDE_BITS)
    // slot_0: 0x13370000
    // slot_1: 0x13371000
    // slot_2: 0x13372000
    // 每个 slot 占一个 page,不共享 cache line,不共享 TLB entry; 每页一个 TLB entry,MAP_POPULATE 预热 TLB
    if (mmap((void *)RB_PTR, ((RB_SLOTS + 1) << RB_STRIDE_BITS), PROT_RW, MMAP_FLAGS, -1, 0) == MAP_FAILED)
    {
        err(1, "rb");
    }

    uint64_t *results_arr[RSB_SIZE] = {results1, results2, results3, results4, results5, results6, results7, results8, results9, results10, results11, results12, results13, results14, results15, results16, results17, results18, results19, results20, results21, results22, results23, results24, results25, results26, results27, results28, results29, results30, results31, results32};

    // 初始化数组
    for (int k = 0; k < RSB_SIZE; k++)
    {
        uint64_t *res = results_arr[k];
        for (int i = 0; i < RB_SLOTS; i++)
        {
            res[i] = 0;
        }
    }

    for (int i = 0; i < ROUNDS; i++)
    {
#if defined(PHT) || defined(BTB)
        // Evict BTB entry belonging to conditional- or indirect jump
        // TODO:为什么这样就可以做到驱逐BTB？.rept, .secret是什么意思？
        // 大量不同 PC 的分支指令：会填满 BTB set（BTB 是 有限容量结构）
        // 生成大量顺序指令;instruction fetch + decode;BTB index 冲突 + replacement
        // 最终生成的是一个超长的 nop 指令序列。让 CPU 跑足够长的“无副作用时间”，以便让预测器 / cache / TLB 等结构自然被冲刷或替换
        asm(
            ".rept 200000000\n\t"
            "nop\n\t"
            ".endr\n\t");
#endif

        // Prime RSB
        // GNU assembler 局部变量;只是 loop counter
        // mfence:保证 之前的所有内存写 已完成
        // lfence:保证 之前的所有指令 执行完成（阻止投机跨越）
        // GAS 汇编局部变量初始化,汇编期符号（assembler directive）,.secret 是一个 汇编器变量,仅用于 .rept 中展开常量
        // 编译时 静态展开:RSB 是基于动态执行的 call/ret 对，不是循环计数器
        // call 4f:将 返回地址（下一条指令地址）压入 RSB + 同时压入 软件栈 (RSP);每一个 call 都会占用 一个 RSB entry
        // .secret 在展开时是：0, 1, 2, ..., 31;add $.secret, %rdi等价于add $i, %rdi;%rdi 用作 索引寄存器
        // shl $12, %rdi;每个 .secret 对应 一个 page
        // mov RB_PTR(%rdi), %r8：effective_addr = RB_PTR + i * 4096;访问 probe array 中 第 i 个 slot;将对应 cache line 加载到 cache
        // 触发 breakpoint 异常;如果没有 debugger;会被信号处理（SIGTRAP）;防止编译器重排
        // 标签 4: 的含义;4f：向前查找最近的 4:;GAS 局部标签机制
        // pop %r8 的作用:
        //     从软件栈：弹出 return address
        //     同时：并不影响 RSB
        asm(
            "mfence\n\t"
            "lfence\n\t"
            ".secret=0\n\t"
            ".rept " xstr(RSB_SIZE) "\n\t"
                                    "call 4f\n\t"
                                    "add $.secret, %%rdi\n\t"
                                    "shl $" xstr(RB_STRIDE_BITS) ", %%rdi\n\t"
                                                                 "mov " xstr(RB_PTR) "(%%rdi), %%r8\n\t"
                                                                                     "int3\n\t"
                                                                                     "4: pop %%r8\n\t"
                                                                                     ".secret=.secret+1\n\t"
                                                                                     ".endr\n\t" ::: "r8");

        flush_range(RB_PTR, 1 << RB_STRIDE_BITS, RB_SLOTS);

#if defined(PHT)
        asm(
            "clflush (%[cond])\n\t"
            "mfence\n\t"
            "mov (%[cond]), %%rdi\n\t"
            "test %%rdi, %%rdi\n\t"
            "je 1f\n\t"
            //"mov "xstr((RB_PTR + (0 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal

            ".rept " xstr(CALLS_CNT) "\n\t"
                                     "call 2f\n\t"
                                     "mov " xstr((RB_PTR + (RSB_SIZE * RB_STRIDE))) ", %%r8\n\t"
                                                                                    "int3\n\t"
                                                                                    "2:\n\t"
                                                                                    ".endr\n\t"

                                                                                    //"mov "xstr((RB_PTR + (1 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal
                                                                                    "int3\n\t"

                                                                                    "1: lfence\n\t" ::[cond] "r"(value) : "r8", "rdi");
#elif defined(RSB)
        asm(
            "call 1f\n\t"
            // "mov "xstr((RB_PTR + (0 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal

            ".rept " xstr(CALLS_CNT) "\n\t"
                                     "call 2f\n\t"
                                     "mov " xstr((RB_PTR + (RSB_SIZE * RB_STRIDE))) ", %%r8\n\t"
                                                                                    "int3\n\t"
                                                                                    "2:\n\t"
                                                                                    ".endr\n\t"

                                                                                    // "mov "xstr((RB_PTR + (1 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal
                                                                                    "int3\n\t" NOPS_str(50) "1: pop %%r8\n\t"
                                                                                                            "pushq $2f\n\t" NOPS_str(1000) "clflush (%%rsp)\n\t"
                                                                                                                                           "mfence\n\t"
                                                                                                                                           "ret\n\t"
                                                                                                                                           "2: lfence\n\t" ::: "r8");
#elif defined(BTB)

        // rdi = 2;攻击循环计数器
        // b_addr → 间接跳转源地址
        // c → BTB 预测目标地址

        // clflush：把 b_addr 指向的内存踢出 cache;保证 flush 完成;让 CPU 无法快速得到真实跳转目标;迫使前端依赖 BTB 预测
        // jmp *(%%r8)：触发 BTB 预测;间接跳转，跳转目标：内存中存的地址;因为 cache miss：BTB 会先给一个预测目标，实际目标稍后解析
        // 再一次 间接跳转，目标是 c，构造 BTB → BTB 链式预测
        // NOP sled（扩大 speculative window）;拉长推测执行窗口

        // 压 return address 到 RSB，RSB 被人为污染/回绕;RSB underflow / wraparound
        // 推测路径访问 RB_PTR + offset;实际架构状态回滚
        asm(
            "mov $2, %%rdi\n\t"
            "mov %[b_addr], %%r8\n\t"
            "mov $c, %%r9\n\t"
            "start_btb:\n\t"

            "clflush (%%r8)\n\t"
            "mfence\n\t"
            "jmp *(%%r8)\n\t"

            "b:\n\t"
            "jmp *%%r9\n\t" NOPS_str(1000) "btb_leak:\n\t"
                                           // "mov "xstr((RB_PTR + (0 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal

                                           ".rept " xstr(CALLS_CNT) "\n\t"
                                                                    "call 4f\n\t"
                                                                    "mov " xstr((RB_PTR + (RSB_SIZE * RB_STRIDE))) ", %%r8\n\t"
                                                                                                                   "lfence\n\t"
                                                                                                                   "4: pop %%r10\n\t"
                                                                                                                   ".endr\n\t"

                                                                                                                   // "mov "xstr((RB_PTR + (1 * RB_STRIDE)))", %%r10\n\t" //Optional indication signal
                                                                                                                   "int3\n\t"
                                                                                                                   "c:\n\t"

                                                                                                                   "mov $btb_leak, %%r9\n\t"
                                                                                                                   "mov %[c_addr], %%r8\n\t"
                                                                                                                   "dec %%rdi\n\t"
                                                                                                                   "cmp $1, %%rdi\n\t"
                                                                                                                   "je start_btb\n\t" ::[b_addr] "r"(&b_addr)`,
            [c_addr] "r"(&c_addr) : "r8", "r9");
#endif

        for (int k = 0; k < RSB_SIZE; k++)
        {
            if (k > 0)
                flush_range(RB_PTR, 1 << RB_STRIDE_BITS, RB_SLOTS);
            // mov $1f, %%r9：把 label 1: 的地址放入 r9;但注意：这里并没有用 call
            // mov $0, %%rdi:清空寄存器（很可能用于：gadget 中的 index;或保持寄存器一致性）
            // 手动向栈中压入一个返回地址;模拟一次 call 1f 的 栈效果;但 RSB 不会因此被更新;RSB 只在 call 指令时更新，而不是在 push 时
            // 把 返回地址所在的 cache line flush 掉;让真实返回地址 慢;强迫 CPU 更依赖 RSB 预测结果
            asm(
                "mfence\n\t"
                "mov $1f, %%r9\n\t"
                "mov $0, %%rdi\n\t"
                "pushq $1f\n\t"
                "clflush (%%rsp)\n\t"
                "ret\n\t"
                "1:\n\t" ::: "r9", "r10", "rdi");
            uint64_t *res = results_arr[k % 32];
            reload_range(RB_PTR, 1 << RB_STRIDE_BITS, RB_SLOTS, res);
        }
    }

    // Print results
    printf("     Return: ");
    for (int i = 0; i < RSB_SIZE; i++)
    {
        printf(" - %04d", i + 1);
    }
    printf("\n");

    for (int i = 0; i < RSB_SIZE; ++i)
    {
        printf("RB entry %02d: ", i);
        for (int k = 0; k < RSB_SIZE; k++)
        {
            uint64_t *res = results_arr[k];
            printf(" - %04ld", res[i]);
        }
        printf("\n");
    }

    printf("   Hijacked: ");
    for (int i = 0; i < RSB_SIZE; i++)
    {
        uint64_t *res = results_arr[i];
        printf(" - %04ld", res[RSB_SIZE]);
    }

    printf("\n");

    return 0;
}
