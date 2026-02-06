#ifndef CACHEUTILS_H
#define CACHEUTILS_H

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

/*
 详细解释这段内联汇编 probe_timing 的设计：
  - movl 指令及寄存器用法：将内存读取结果放入 eax，再将 eax 复制到 esi 以便做时间差对比。
  - mfence 与 lfence：用于避免指令重排序，确保 rdtsc 的时间戳精确测量。
  - rdtsc：读取时间戳计数器，配合 lfence，得到精确的单个指令执行时间。
  - subl：在 x86 AT&T 语法下做有符号减法，计算 delta 时间。
  - clflush：清除指定内存地址的缓存行，确保后续访问会触发缓存未命中以用于测量。
  - 0(%1) 与 %1 的使用：访问传入地址 adrs 指向的内存，( %1 ) 指向该参数所绑定的寄存器/内存地址。
  该片段的设计核心是通过对缓存命中/未命中的时间差来推断缓存行为，从而在加密实现中进行侧信道分析。
*/
unsigned long probe_timing(char *adrs)
{
  volatile unsigned long time;

  asm __volatile__(
      "    mfence             \n"
      "    lfence             \n"
      "    rdtsc              \n"
      "    lfence             \n"
      "    movl %%eax, %%esi  \n"
      "    movl (%1), %%eax   \n"
      "    lfence             \n"
      "    rdtsc              \n"
      "    subl %%esi, %%eax  \n"
      "    clflush 0(%1)      \n"
      : "=a"(time)
      : "c"(adrs)
      : "%esi", "%edx");
  return time;
}

unsigned long long rdtsc()
{
  unsigned long long a, d;
  asm volatile("mfence");
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
  a = (d << 32) | a;
  asm volatile("mfence");
  return a;
}

/*
 约束符号解释：
  - "c"(p) 是内联汇编中的输入操作数约束，表示把 C 语言变量 p 的值绑定到指定的寄存器（在 AT&T 语法下通常是 %rcx）。
  - 这使得汇编片段可以通过该寄存器访问传入的地址 p。
  - 与之对应的 ":" 部分列出输入寄存式的操作数，确保编译器正确地分配寄存器。
*/
void maccess(void *p)
{
  asm volatile("movq (%0), %%rax\n"
               :
               : "c"(p)
               : "rax");
}

void flush(void *p)
{
  asm volatile("clflush 0(%0)\n"
               :
               : "c"(p)
               : "rax");
}

#endif
