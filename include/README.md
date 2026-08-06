# xcc C89 headers

Hosted-environment declaration headers for glibc on x86-64 Linux. xcc finds
these headers relative to its executable and links with the system C library
(`gcc`). User `-I` directories are searched before these headers.

## Provided (C89)

| Header | Role |
| --- | --- |
| `<stddef.h>` | `size_t`, `ptrdiff_t`, `wchar_t`, `NULL`, `offsetof` |
| `<limits.h>` | Implementation limits (`CHAR_BIT`, `INT_MAX`, …) |
| `<float.h>` | Floating characteristics (`FLT_MAX`, `LDBL_MANT_DIG`, …) |
| `<errno.h>` | `errno`, `EDOM`, `ERANGE`, common Linux `E*` codes |
| `<string.h>` | String and memory function declarations |
| `<stdlib.h>` | Memory, conversion, sort, `div_t`, multibyte declarations |
| `<ctype.h>` | Character classification declarations |
| `<math.h>` | `double` math declarations (`-lm` at link time) |
| `<time.h>` | `time_t`, `struct tm`, clock and calendar declarations |
| `<locale.h>` | `setlocale`, `localeconv`, `struct lconv` |
| `<assert.h>` | `assert` macro (uses `abort` from `<stdlib.h>`) |
| `<setjmp.h>` | `setjmp` / `longjmp` (glibc-compatible `jmp_buf` layout) |
| `<signal.h>` | `signal`, `raise`, `SIG*` constants |
| `<stdarg.h>` | `va_list`, `va_start`, `va_arg`, `va_end` |
| `<stdio.h>` | Streams, formatted I/O, file positioning, and buffering |

## Not provided

All C89 headers are now provided.

Non-C89 headers (`<stdint.h>`, `<stdbool.h>`, …) are intentionally omitted.

`struct tm`, `struct lconv`, `jmp_buf`, and `errno` use glibc-compatible
layouts/macros so calls into the system C library remain ABI-safe on Linux.

## setjmp / longjmp

`<setjmp.h>` declares glibc's `_setjmp` / `longjmp` and defines
`setjmp(env)` as `_setjmp(env)`. That is not just an ABI detail: **xcc
special-cases direct calls to those libc symbols** in the backend.

`_setjmp` snapshots machine state (stack pointer, callee-saved GPRs, etc.).
`longjmp` restores that snapshot and resumes as if `_setjmp` had returned a
second time. Ordinary SSA promotion and register allocation break here: a local
kept in `%rbx` is rolled back to its value at the `_setjmp` call, and mem2reg
can split one C variable into several virtual values that no longer share a
single stack home after a non-local jump.

To keep hosted setjmp tests correct, xcc currently:

1. **liveness** — blocks callee-saved physical registers at `_setjmp` /
   `longjmp` call sites so live locals are spilled to the stack.
2. **regalloc** — refuses callee-saved registers and spill fragmentation for
   intervals that span those calls.
3. **mem2reg** — skips promotion for every local in a function that contains
   `_setjmp` or `longjmp` (conservative but safe).

Recognition is by **direct call name** (`_setjmp`, `longjmp`) via
`lir_call_is_setjmp_family()` in `src/lir.c`. Indirect calls, `sigsetjmp`, or
wrappers with other symbol names are not handled yet. This is an intentional,
documented hack tied to this header and glibc on x86-64 Linux; see also the
main [README](../README.md#setjmp--longjmp).
