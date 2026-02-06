#!/usr/bin/env python3

N = 2048

with open("gen_branches.h", "w") as f:
    f.write("/* Auto-generated BTB branch functions */\n")
    f.write("#pragma once\n\n")

    for i in range(N):
        f.write(f"""
__attribute__((noinline))
void branch_{i}(void (*target)(void)) {{
    asm volatile("" ::: "memory");
    target();
}}
""")

with open("branch_list.h", "w") as f:
    for i in range(N):
        f.write(f"X({i})\n")

print(f"[+] Generated gen_branches.h and branch_list.h with {N} branches")
