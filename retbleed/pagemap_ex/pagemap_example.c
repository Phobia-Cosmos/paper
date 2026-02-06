#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>

#define PAGE_SIZE 4096

typedef uint64_t u64;

/**
 * pagemap 结构 (从内核文档 Documentation/vm/pagemap.txt)
 *
 * 每个虚拟页面对应一个64位条目:
 *
 *  Bits 0-54: 页帧号 (PFN) - physical frame number
 *  Bit  55:    页是否在RAM中 (0=不在RAM, 1=在RAM)
 *  Bits 56-60: 保留 (目前未使用)
 *  Bit  61:    页被换出到交换空间
 *  Bit  62:    页是文件映射或共享内存
 *  Bit  63:    页存在于RAM中 (同bit 55)
 */
#define _PAGE_PRESENT  (1ULL << 63)
#define _PAGE_SWAPPED  (1ULL << 61)
#define _PAGE_FILE     (1ULL << 62)
#define _PAGE_PSWANGED (1ULL << 60)
#define _PAGE_MODIFIED _PAGE_PSWANGED
#define _PAGE_ACCESSED 0x0200
#define _PAGE_DIRTY    0x0400
#define _PAGE_PFN      0x007FFFFFFFFFFFFF

/**
 * va_to_phys: 将虚拟地址转换为物理地址
 *
 * @fd_pagemap: 打开的 /proc/self/pagemap 文件描述符
 * @va:         虚拟地址
 *
 * 返回: 物理地址 (64位), 0 表示失败或页不在RAM中
 */
static u64 va_to_phys(int fd_pagemap, u64 va)
{
    u64 pagemap_entry;
    u64 offset;
    ssize_t ret;

    if (fd_pagemap < 0) {
        return 0;
    }

    // 计算该虚拟地址在pagemap文件中的偏移量
    // 每个条目8字节，偏移量 = (虚拟地址 / 页大小) * 8
    offset = (va / PAGE_SIZE) * sizeof(u64);

    // lseek到对应位置
    if (lseek(fd_pagemap, offset, SEEK_SET) != (off_t)offset) {
        perror("lseek");
        return 0;
    }

    // 读取pagemap条目
    ret = read(fd_pagemap, &pagemap_entry, sizeof(pagemap_entry));
    if (ret != sizeof(pagemap_entry)) {
        perror("read pagemap");
        return 0;
    }

    // 检查页是否存在
    if (!(pagemap_entry & _PAGE_PRESENT)) {
        printf("va=0x%lx: 页不在RAM中 (可能已被换出)\n", va);
        return 0;
    }

    // 提取PFN (页帧号)
    u64 pfn = pagemap_entry & _PAGE_PFN;

    // 物理地址 = PFN * 页大小 + 页内偏移
    u64 pa = (pfn * PAGE_SIZE) + (va % PAGE_SIZE);

    printf("va=0x%lx -> pfn=0x%lx -> pa=0x%lx\n", va, pfn, pa);

    return pa;
}

/**
 * page_offset_base_addr: 获取 page_offset_base
 *
 * page_offset_base 是内核符号，表示内核直接映射区域的起始地址
 * 例如: 0xffff888000000000 (x86_64)
 *
 * 获取方法:
 * 1. 从 /proc/kallsyms 读取
 * 2. 从 System.map 文件读取
 * 3. 通过已知的内核地址反推
 */
static u64 get_page_offset_base(void)
{
    FILE *fp;
    char line[256];
    u64 addr = 0;

    // 方法1: 从 /proc/kallsyms 读取
    // 需要 root 权限
    fp = fopen("/proc/kallsyms", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            // 格式: "address type symbol"
            // 例如: "ffff888000000000 T page_offset_base"
            if (strstr(line, "page_offset_base")) {
                sscanf(line, "%lx", &addr);
                printf("从 /proc/kallsyms 获取 page_offset_base = 0x%lx\n", addr);
                break;
            }
        }
        fclose(fp);
    } else {
        perror("无法打开 /proc/kallsyms");
    }

    return addr;
}

/**
 * 示例2: 遍历进程的虚拟地址空间
 */
static void walk_vma(int fd_pagemap)
{
    FILE *fp;
    char line[512];
    u64 start, end;
    char perms[5] = {0};
    unsigned long offset, major, minor, inode;
    char pathname[256] = {0};

    // 读取 /proc/self/maps
    fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        perror("fopen maps");
        return;
    }

    printf("\n=== 进程虚拟地址空间 (前10个区域) ===\n");
    printf("%-18s %-18s %-5s %-12s %s\n",
           "start", "end", "perms", "offset", "pathname");

    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 10) {
        // 解析maps格式:
        // start-end perms offset major:minor inode pathname
        sscanf(line, "%lx-%lx %4s %lx %lx:%lx %lu %255s",
               &start, &end, perms, &offset, &major, &minor, &inode, pathname);

        // 只显示有文件映射的区域
        if (pathname[0] == '/' || strstr(pathname, ".so")) {
            // 尝试将映射区域的起始地址转换为物理地址
            u64 pa = va_to_phys(fd_pagemap, start);
            printf("0x%016lx-0x%016lx %-5s 0x%-10lx %s\n",
                   start, end, perms, offset, pathname);
            count++;
        }
    }
    fclose(fp);
}

/**
 * 示例3: 验证物理地址连续性
 */
static void test_consecutive_pages(int fd_pagemap)
{
    char *buf;
    int i;

    printf("\n=== 验证物理地址连续性 ===\n");

    // 分配 4 个连续页
    buf = aligned_alloc(PAGE_SIZE, 4 * PAGE_SIZE);
    if (!buf) {
        perror("aligned_alloc");
        return;
    }

    // 确保映射到物理内存
    memset(buf, 0xAA, 4 * PAGE_SIZE);

    printf("虚拟地址: 0x%lx\n", (u64)buf);

    // 转换4个页的物理地址
    for (i = 0; i < 4; i++) {
        u64 va = (u64)buf + i * PAGE_SIZE;
        u64 pa = va_to_phys(fd_pagemap, va);

        if (pa == 0) {
            continue;
        }

        // 验证物理地址是否连续 (相差 PAGE_SIZE)
        if (i > 0) {
            u64 prev_pa = va_to_phys(fd_pagemap, (u64)buf + (i-1) * PAGE_SIZE);
            if (pa == prev_pa + PAGE_SIZE) {
                printf("  页%d: 物理地址连续 ✓\n", i);
            } else {
                printf("  页%d: 物理地址不连续 ✗\n", i);
            }
        }
    }

    free(buf);
}

/**
 * 示例4: 演示 copy_to_user / copy_from_user 的原理
 *
 * 这些函数用于内核空间和用户空间之间的数据拷贝
 *
 * copy_to_user(dst_user, src_kernel, n):
 *   - 从内核空间 src 拷贝 n 字节到用户空间 dst
 *   - 返回未能拷贝的字节数 (0表示成功)
 *
 * copy_from_user(dst_kernel, src_user, n):
 *   - 从用户空间 src 拷贝 n 字节到内核空间 dst
 *   - 返回未能拷贝的字节数
 *
 * 为什么需要这些函数?
 * - 用户空间和内核空间有地址隔离
 * - 用户地址在物理上可能不存在 (未映射)
 * - 需要检查用户地址的有效性
 * - 需要处理页错误
 *
 * 内核内部实现类似:
 *   copy_to_user(void *dst, void *src, unsigned long n):
 *       unsigned long ret;
 *       // 1. 检查 dst 是否是用户空间地址
 *       // 2. 触发页面错误，将页加载到内存
 *       // 3. 拷贝数据 (可能需要多次拷贝，处理未对齐)
 *       // 4. 返回未拷贝的字节数
 *       return ret;
 */
static void demonstrate_copy_functions(void)
{
    printf("\n=== copy_to_user / copy_from_user 说明 ===\n\n");

    printf("copy_to_user(dst_user, src_kernel, n):\n");
    printf("  用途: 从内核空间拷贝数据到用户空间\n");
    printf("  返回: 未拷贝的字节数 (0=成功, n=完全失败)\n");
    printf("  示例:\n");
    printf("    struct synth_gadget_desc desc = {...};\n");
    printf("    if (copy_to_user(user_ptr, &desc, sizeof(desc)) != 0)\n");
    printf("        return -EFAULT;\n\n");

    printf("copy_from_user(dst_kernel, src_user, n):\n");
    printf("  用途: 从用户空间拷贝数据到内核空间\n");
    printf("  返回: 未拷贝的字节数\n");
    printf("  示例:\n");
    printf("    struct payload p;\n");
    printf("    if (copy_from_user(&p, user_ptr, sizeof(p)) != 0)\n");
    printf("        return -EFAULT;\n\n");

    printf("安全性:\n");
    printf("  - 检查用户地址是否有效\n");
    printf("  - 捕获页错误，处理未映射的页\n");
    printf("  - 防止内核访问非法内存\n");
}

/**
 * 示例5: proc_ops 结构说明
 */
static void demonstrate_proc_ops(void)
{
    printf("\n=== proc_ops 结构说明 ===\n\n");

    printf("proc_ops 是 proc 文件系统的操作函数表:\n\n");

    printf("struct proc_ops {\n");
    printf("    int (*proc_open)(struct inode *, struct file *);\n");
    printf("    int (*proc_release)(struct inode *, struct file *);\n");
    printf("    ssize_t (*proc_read)(struct file *, char __user *, size_t, loff_t *);\n");
    printf("    ssize_t (*proc_write)(struct file *, const char __user *, size_t, loff_t *);\n");
    printf("    int (*proc_ioctl)(struct file *, unsigned int, unsigned long);\n");
    printf("    ...\n");
    printf("};\n\n");

    printf("cp_bti 中的使用:\n");
    printf("  static struct proc_ops pops = {\n");
    printf("      .proc_ioctl = handle_ioctl,\n");
    printf("      .proc_open = nonseekable_open,\n");
    printf("      .proc_lseek = no_llseek,\n");
    printf("  };\n\n");

    printf("当用户空间调用 open(\"/proc/retbleed_poc\") 时:\n");
    printf("  1. 内核分配 struct file 结构\n");
    printf("  2. 调用 pops.proc_open (如果定义了)\n");
    printf("  3. 返回文件描述符给用户\n\n");

    printf("当用户空间调用 ioctl(fd, REQ_GADGET, arg) 时:\n");
    printf("  1. 内核查找文件的 proc_ops\n");
    printf("  2. 调用 pops.proc_ioctl\n");
    printf("  3. 执行 handle_ioctl() 函数\n");
    printf("  4. 返回结果给用户\n");
}

int main(int argc, char *argv[])
{
    int fd_pagemap;
    u64 page_offset_base;

    printf("=== pagemap 使用示例 ===\n\n");

    // 需要 root 权限
    fd_pagemap = open("/proc/self/pagemap", O_RDONLY);
    if (fd_pagemap < 0) {
        fprintf(stderr, "需要 root 权限运行: sudo %s\n", argv[0]);
        return 1;
    }

    printf("已打开 /proc/self/pagemap (fd=%d)\n\n", fd_pagemap);

    // 获取 page_offset_base
    page_offset_base = get_page_offset_base();
    if (page_offset_base == 0) {
        printf("警告: 无法获取 page_offset_base\n");
        printf("      这是内核符号，需要 root 权限\n");
        printf("      Retbleed POC 通过内核模块获取此值\n\n");
    }

    // 示例1: 转换栈地址
    char stack_var = 'X';
    printf("=== 示例1: 转换栈地址 ===\n");
    printf("栈变量地址: 0x%lx, 值: '%c'\n", (u64)&stack_var, stack_var);
    va_to_phys(fd_pagemap, (u64)&stack_var);

    // 示例2: 遍历VMA
    walk_vma(fd_pagemap);

    // 示例3: 验证连续页
    test_consecutive_pages(fd_pagemap);

    // 示例4: copy函数说明
    demonstrate_copy_functions();

    // 示例5: proc_ops说明
    demonstrate_proc_ops();

    // 清理
    close(fd_pagemap);

    printf("\n=== 完成 ===\n");
    return 0;
}
