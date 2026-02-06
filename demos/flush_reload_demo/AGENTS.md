AGENTS.md
This document provides practical guidelines for AI agents operating in this codebase. It focuses on build, lint, test workflows and code‑style expectations to enable fast, repeatable, and maintainable changes.

Overview
- Target language: C (project examples include attacker.c and victim.c).
- Intent: help agents perform changes that are correct, well‑documented, and testable. It includes commands for building, linting, and testing, plus coding standards.

Build, Lint, Test Commands
- Build (prefer project build system when present; fall back to direct compiler invocations):
  - If a Makefile exists: make -j
  - If a CMake project exists: mkdir -p build; cd build; cmake ..; cmake --build .
  - If a Ninja–based setup exists: cmake -S . -B build -G Ninja; cmake --build build
- Run unit tests:
  - If using CMake/CTest: cd build; ctest -V
  - If tests are standalone binaries: ./build/tests/<test_name> [args]
- Run a single test quickly (GoogleTest/CTest examples):
  - CTest: ctest -R <test_name> -V
  - GoogleTest binary: ./test_binary --gtest_filter=<TestSuite>.<TestName>
- Lint and static analysis:
  - clang-tidy: require compile_commands.json; run: clang-tidy <files> -- -I./include
  - cppcheck: cppcheck --enable=all --suppress=missingIncludeSystem .
- Formatting:
  - clang-format -i $(git ls-files '*.c' '*.h')  # apply to tracked sources
- Sanitizers for debugging:
  - ASAN: export ASAN_OPTIONS=detect_leaks=1; gcc -fsanitize=address -fno-omit-frame-pointer -O1 -g ...
- Quick-start template (copy/paste):
  - mkdir build; cd build; cmake ..; cmake --build .; ctest -V

Code Style Guidelines
- General philosophy: write clear, maintainable C code with emphasis on correctness and safety.
- Formatting:
  - 4 spaces per indent; 120 character line limit (adjust if project requires); no trailing whitespace.
  - Braces: K&R style for functions; opening brace on same line as declaration/statement.
  - Include guards: header files use PROJECTNAME_H or PROJECTNAME_YYY_H patterns.
- Includes:
  - System headers first, then blank line, then project headers.
  - Use #include <...> for system; #include "..." for local headers.
- Headers and modules:
  - Each .h should be self-contained; protect with #ifndef / #define / #endif guards.
  - Public API should be documented in Doxygen style where practical.
- Types and naming:
  - Use stdint.h and bool where appropriate; prefer fixed-width integers for portability (uint32_t, int64_t).
  - Naming: functions and variables use snake_case; types/classes use CamelCase; constants use ALL_CAPS_WITH_UNDERSCORES.
- Function design:
  - Small, focused functions; single responsibility; avoid long parameter lists.
  - Prefer explicit error codes; return 0 on success, non‑zero on failure; propagate errors upward.
- Error handling:
  - Propagate errno or explicit error codes; include context when returning errors.
  - Do not swallow errors; log or surface errors to caller.
- Memory safety:
  - Check all malloc/free; prefer calloc; pair allocations with corresponding frees; consider smart patterns in C (ownership comments).
- Concurrency:
  - If using threads, document ownership and synchronization; use pthreads or a chosen library consistently.
- Testing and debugging:
  - Add small unit tests where feasible; use a test harness directory (tests/).
  - Compile with -g for debugging; consider -fsanitize=address,undefined when debugging.
- Documentation:
  - Prefer Doxygen-style comments for public APIs; inline comments for tricky logic.
- Build hygiene:
  - Commit only code, not generated artifacts; include a build script or a small README in build/ for repeatability.

Agent Workflow Notes
- When performing changes use a minimal patch with a clear rationale; describe why the change is needed.
- Prefer incremental changes with focused tests; avoid large rewrites without explicit request.
- If the repo uses a specific test naming convention, preserve it in new tests.
- Reproduce failures locally using the exact commands listed above; capture failure output for the PR description.

Cursor Rules (if present)
- Cursor rules directory: .cursor/rules
- If rules exist, quote their purpose and ensure agent actions comply. If not present, note that no cursor rules were found.

Copilot Rules (if present)
- Copilot rules file: .github/copilot-instructions.md
- If present, summarize how Copilot should be engaged for this repository and honor any explicit instructions.

Notes
- This file is consumed by automated agents; keep wording precise and avoid excessive prose.
- For any suggested change, provide a short justification and a linked reference to related files.
