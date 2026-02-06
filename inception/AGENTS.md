# AGENTS.md

This file contains guidelines for agents working with the Inception codebase, a research project on transient execution attacks on AMD Zen CPUs.

## Build Commands

### Common Targets

Each subproject (tte_btb, tte_rsb, phantomcall, inception, ibpb-eval) has its own build system:

```bash
# Build all binaries in tte_btb
cd tte_btb && make clean all

# Build all binaries in tte_rsb
cd tte_rsb && make clean

# Build phantomcall (subdirectory specific)
cd phantomcall/zen_1_2 && make clean all
cd phantomcall/zen_3_4 && make clean all

# Build inception (subdirectory specific)
cd inception/zen_1_2 && make clean all
cd inception/zen_4 && make clean all

# Build ibpb-eval
cd ibpb-eval/ibpb_cost && make clean all
```

### Compiler and Flags

- **Compiler**: `clang` (set via `CC = clang` in Makefiles)
- **Common flags**: `-O2 -Wall -Wno-language-extension-token -Wno-unused-function -pedantic`
- **Architecture**: Determined by `arch.sh` which maps hostname to microarchitecture (ZEN1, ZEN2, ZEN3, ZEN4, COFFEE, etc.)
- **Kernel modules**: Some components require building kernel modules (kmod directories) with kernel build system

### Running Experiments

```bash
# TTE BTB experiments
cd tte_btb && ./run_all.sh

# TTE RSB experiments
cd tte_rsb && ./tte_rsb.sh CORE1 CORE2 OUTPUT_DIR [CLANG_ARGS]

# PhantomCall experiments
cd phantomcall/zen_1_2 && ./recursive_pcall.sh ZEN 1 9 out
cd phantomcall/zen_3_4 && ./recursive_pcall.sh ZEN3 1 9 out

# Inception data leak
cd inception/zen_1_2 && ./inception ADDRESS
cd inception/zen_4 && ./inception ADDRESS

# Check system compatibility
./check.sh
```

## Code Style Guidelines

### General Principles

- This is performance-critical security research code
- Inline assembly and low-level manipulation are expected
- Code is architecture-specific and often targets specific AMD Zen microarchitectures
- Prefer explicit over implicit; make side effects visible

### Imports and Headers

- Standard C library includes first (alphabetical within groups):
  ```c
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  ```
- Local project headers next:
  ```c
  #include "memtools.h"
  #include "rb_tools.h"
  ```
- Feature/test defines before includes:
  ```c
  #define _GNU_SOURCE
  #include <fcntl.h>
  ```

### Naming Conventions

- **Types**: Use `u64`, `u8` for unsigned types (defined in headers):
  ```c
  typedef unsigned long u64;
  typedef unsigned char u8;
  ```
- **Structs**: CamelCase with leading capital:
  ```c
  struct j_malloc { ... };
  ```
- **Functions**: snake_case:
  ```c
  int map_exec(struct j_malloc *m, u64 addr, u64 code_sz);
  ```
- **Constants/Macros**: SCREAMING_SNAKE_CASE:
  ```c
  #define RB_STRIDE 0x10000
  #define REPS 1000000
  ```
- **Macros for code generation**: Use `mk_tmpl`, `tmpl_sz`, `alloc_tmpl` patterns

### Formatting

- **Indentation**: Tabs (not spaces) for indentation
- **Line length**: No strict limit; prioritize readability
- **Braces**: K&R style for function definitions:
  ```c
  int func(int arg)
  {
      if (cond) {
          do_something();
      }
      return 0;
  }
  ```
- **Spacing**: Spaces around operators in macros and complex expressions
- **Assembly blocks**: Use `asm` or `asm volatile` with proper memory clobbers:
  ```c
  asm volatile("CPUID\n\t"
               "RDTSC\n\t"
               "movq %%rdx, %0\n\t"
               "movq %%rax, %1\n\t" : "=r"(hi), "=r"(lo)::"%rax", "%rbx", "%rcx", "%rdx");
  ```

### Functions

- Use `static inline` or `static __always_inline` for performance-critical functions
- Mark performance-sensitive functions with `__attribute__((always_inline))`
- Keep functions focused and short; use helper functions for repeated patterns
- Document function behavior with comments for complex logic:
  ```c
  /**
   * makes an executable mapping.
   * returns 0 on success, -1 otherwise
   */
  int map_exec(struct j_malloc *m, u64 addr, u64 code_sz);
  ```

### Error Handling

- Return values for error reporting:
  - `0` or positive for success
  - `-1` for failure
- Use `err(1, "message")` from `<err.h>` for fatal errors in initialization
- Check return values of `mmap`, `mprotect`, `munmap`:
  ```c
  if (mmap(m->map_base, m->map_sz, PROT_RWX, MMAP_FLAGS, -1, 0) == MAP_FAILED) {
      return -1;
  }
  ```
- Use `siglongjmp`/`sigsetjmp` for handling segmentation faults in controlled ways

### Performance-Critical Code

- Use `__asm__ volatile` with proper constraints for inline assembly
- Use memory barriers (`mfence`, `lfence`, `sfence`) explicitly
- Flush cache lines with `clflush` or `clflushopt`
- Use `rdtsc`/`rdtscp` for timing measurements
- Consider cache timing side channels in attack code

### Architecture-Specific Code

- Use preprocessor conditionals for microarchitecture differences:
  ```c
  #if defined(ZEN2)
  #define PTRN 0xffff800800000000
  #else
  #define PTRN 0xffff804000000000
  #endif
  ```
- Define architecture via `arch.sh` script (sets `ARCH` environment variable)
- Code organized by architecture: `zen_1_2/`, `zen_4/`, etc.

### Code Generation Patterns

- Use macro-based code templating for generating machine code at compile time:
  ```c
  #define mk_tmpl(name, str)\
      extern char name##__tmpl[]; \
      extern char e_##name##__tmpl[]; \
      asm(".align 0x1000\n"\
          #name"__tmpl:\n"\
          str "\n"\
          "e_"#name"__tmpl: nop")
  ```
- Template sizes computed at compile time:
  ```c
  #define tmpl_sz(name) (unsigned long) (e_##name##__tmpl - name##__tmpl)
  ```

### Testing

- This is research/benchmark code with no formal unit tests
- Experiments are run via shell scripts that execute binaries and collect output
- Verify system compatibility with `./check.sh` before running experiments
- Expected behavior is documented in README files in each subdirectory

### Security Considerations

- This code demonstrates transient execution attacks (side channels)
- Do not modify attack gadget addresses without understanding the target
- Be careful with memory permissions (RWX mappings)
- Understand that this code intentionally induces speculative execution

### Key Files

- `tte_btb/`: BTB manipulation experiments
- `tte_rsb/`: RSB manipulation experiments
- `phantomcall/`: Recursive PhantomCALL experiments
- `inception/`: Main Inception attack implementation
- `ibpb-eval/`: IBPB mitigation evaluation
- `check.sh`: System compatibility check
