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

`<setjmp.h>` provides glibc-compatible storage and maps the public macros to
`__builtin_setjmp` and `__builtin_longjmp`. Sema validates the builtin
arguments; lowering emits calls to glibc's `_setjmp` and `longjmp` symbols.

`_setjmp` snapshots machine state (stack pointer, callee-saved GPRs, etc.).
`longjmp` restores that snapshot and resumes as if `_setjmp` had returned a
second time. The calls carry generic LIR effects: `setjmp` is `returns-twice`,
and `longjmp` is `noreturn` and ends its normal CFG path.

The normal optimizer and allocator rules remain valid under the C89 contract:

- An unchanged automatic local retains its value.
- An automatic non-`volatile` local modified between `setjmp` and `longjmp`
  has an indeterminate value afterward.
- A modified local whose value must survive must be declared `volatile`; xcc
  already excludes volatile locals from mem2reg.

This removes the former libc-name checks from liveness, regalloc, and mem2reg.
The call-effect representation is generic so future declarations or attributes
can use it without adding backend symbol-name checks. `sigsetjmp` remains
outside the C89 header surface. See also the main
[README](../README.md#setjmp--longjmp).
