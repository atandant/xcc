# xcc Builtins

This document covers the builtin interfaces recognized by xcc. There are two
separate builtin surfaces:

- compiler expression builtins named `__builtin_*`, parsed by the C compiler;
- predefined preprocessor macros such as `__LINE__` and `__XCC__`.

Most user code should prefer the public macros in the implementation headers
under `include/` and compile with `-Iinclude`. The direct `__builtin_*` forms
are documented here because they are part of xcc's accepted source language and
are useful when writing low-level headers or tests.

## Quick reference

| Builtin | Public macro | Header | Result | Purpose |
| --- | --- | --- | --- | --- |
| `__builtin_va_start(ap, last)` | `va_start(ap, last)` | `<stdarg.h>` | `void` | Initialize a `va_list` in a variadic function. |
| `__builtin_va_arg(ap, type)` | `va_arg(ap, type)` | `<stdarg.h>` | `type` | Fetch the next variadic argument as `type`. |
| `__builtin_va_end(ap)` | `va_end(ap)` | `<stdarg.h>` | `void` | End use of a `va_list`. |
| `__builtin_setjmp(env)` | `setjmp(env)` | `<setjmp.h>` | `int` | Save non-local jump state. |
| `__builtin_longjmp(env, value)` | `longjmp(env, value)` | `<setjmp.h>` | `void` | Restore state saved by `setjmp`. |
| `__builtin_offsetof(type, member)` | `offsetof(type, member)` | `<stddef.h>` | `unsigned long` constant | Compute the byte offset of a struct or union member. |
| `__builtin_huge_val()` | `HUGE_VAL` | `<math.h>` | floating constant | Produce positive infinity for `double` contexts. |

## General rules

`__builtin_*` names are lexer keywords in xcc, not ordinary functions. They do
not need prototypes and cannot be redeclared to change their behavior.

The implementation is currently for x86-64 Linux and follows the System V AMD64
ABI where that matters. The hosted headers use glibc-compatible layouts for
`va_list` and `jmp_buf` so generated programs can link against the system C
library.

The implementation headers are not included automatically. Use:

```sh
./xcc -Iinclude program.c
```

or include the direct builtin names yourself in low-level code.

## Variadic builtins

xcc implements C variadic argument access through the System V AMD64 `va_list`
layout:

```c
typedef struct __xcc_va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];
```

The public macros in `<stdarg.h>` expand directly to the builtin forms:

```c
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
```

### `__builtin_va_start(ap, last)`

Syntax:

```c
__builtin_va_start(ap, last)
```

Initializes `ap` so later `__builtin_va_arg` operations read the unnamed
arguments passed through `...`.

Requirements:

- `ap` must have pointer type. In normal code it is a `va_list` object from
  `<stdarg.h>`, which decays to a pointer when passed to the builtin.
- The builtin must be used inside a variadic function.
- `last` must name the function's rightmost named parameter.
- The rightmost named parameter must not be declared `register`.

Example:

```c
#include <stdarg.h>

int sum(int n, ...)
{
    va_list ap;
    int total = 0;
    int i;

    va_start(ap, n);
    for (i = 0; i < n; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}
```

Lowering initializes the four SysV AMD64 `va_list` fields: general-purpose
register offset, floating-point register offset, overflow stack area, and the
register save area.

### `__builtin_va_arg(ap, type)`

Syntax:

```c
__builtin_va_arg(ap, type)
```

Reads the next argument from `ap`, advances `ap`, and yields a value of
`type`.

Requirements:

- `ap` must have pointer type.
- `type` must be complete.
- `type` must not be `void`.
- `type` must not be an array type.
- `type` must not be a function type.
- Do not request a type that would have been promoted when passed through
  `...`: xcc rejects integer types narrower than `int` and rejects `float`.
- Structs and unions may be used if xcc supports passing them by value for the
  current ABI subset. A struct or union containing floating members is rejected.

Examples:

```c
long take_long(int tag, ...)
{
    va_list ap;
    long value;

    va_start(ap, tag);
    value = va_arg(ap, long);
    va_end(ap);
    return value;
}
```

```c
struct Pair { int a; int b; };

int take_pair(int tag, ...)
{
    va_list ap;
    struct Pair p;

    va_start(ap, tag);
    p = va_arg(ap, struct Pair);
    va_end(ap);
    return p.a + p.b;
}
```

Notes:

- `double` variadic arguments use the floating-point register save area until
  those slots are exhausted, then the overflow stack area.
- `long double` variadic arguments are read from the overflow stack area with
  16-byte alignment.
- Integer and pointer arguments use general-purpose slots until those slots are
  exhausted, then the overflow stack area.
- Supported by-value records use ABI classification to choose register save
  area versus overflow stack area.

### `__builtin_va_end(ap)`

Syntax:

```c
__builtin_va_end(ap)
```

Ends use of `ap`. xcc type-checks the `va_list` expression and lowers this to
no runtime cleanup, matching the current SysV AMD64 representation.

Requirements:

- `ap` must have pointer type.

You may restart a `va_list` by calling `__builtin_va_start` again after
`__builtin_va_end`.

## Non-local control flow builtins

`<setjmp.h>` maps the public macros to xcc builtins:

```c
#define setjmp(env) __builtin_setjmp(env)
#define longjmp(env, val) __builtin_longjmp(env, val)
```

The header defines `jmp_buf` with a glibc-compatible x86-64 layout. xcc lowers
the builtins to glibc symbols:

- `__builtin_setjmp` lowers to `_setjmp`;
- `__builtin_longjmp` lowers to `longjmp`.

### `__builtin_setjmp(env)`

Syntax:

```c
int result = __builtin_setjmp(env);
```

Saves the current non-local jump state into `env` and returns `0` on the initial
call. If a later `__builtin_longjmp` restores the same environment, execution
resumes at the `__builtin_setjmp` expression and it returns the value supplied
to `__builtin_longjmp`.

Requirements:

- `env` must have pointer type. In normal code this is a `jmp_buf` object from
  `<setjmp.h>`, which decays to a pointer when passed to the builtin.

Example:

```c
#include <setjmp.h>

jmp_buf env;

int f(void)
{
    if (setjmp(env) == 0)
        longjmp(env, 7);
    return 7;
}
```

Compiler behavior:

- The call is marked `returns-twice`.
- Optimizer and register allocator behavior follows the C89 rule: an automatic
  non-`volatile` local modified after `setjmp` has an indeterminate value after
  `longjmp`. Use `volatile` for a modified local whose value must survive.

### `__builtin_longjmp(env, value)`

Syntax:

```c
__builtin_longjmp(env, value)
```

Restores the jump state saved in `env`. It does not return normally.

Requirements:

- `env` must have pointer type.
- `value` must have integer type. xcc converts it to `int`.

Compiler behavior:

- The builtin has type `void`.
- The call is marked `noreturn`.
- The current control-flow path has no normal successor after the call.

## `__builtin_offsetof(type, member)`

Syntax:

```c
__builtin_offsetof(type, member)
```

Computes the byte offset of a member inside a complete struct or union type.
`<stddef.h>` exposes this as:

```c
#define offsetof(type, member) __builtin_offsetof(type, member)
```

Requirements:

- `type` must be a complete `struct` or `union` type.
- `member` must name a valid member of that type.
- Nested member designators using `.` are accepted.
- A bit-field member is rejected.

Examples:

```c
#include <stddef.h>

struct Header {
    int tag;
    long size;
};

int offset_of_size(void)
{
    return offsetof(struct Header, size);
}
```

```c
struct Outer {
    int tag;
    struct Inner {
        char c;
        long value;
    } inner;
};

unsigned long off = __builtin_offsetof(struct Outer, inner.value);
```

The result is produced as an integer constant with `unsigned long` suffix
metadata, so it is suitable for constant-expression use.

## `__builtin_huge_val()`

Syntax:

```c
__builtin_huge_val()
```

Produces a floating constant representing positive infinity. `<math.h>` exposes
this as:

```c
#define HUGE_VAL (__builtin_huge_val())
```

Example:

```c
#include <math.h>

double overflow_value(void)
{
    return HUGE_VAL;
}
```

The builtin takes no arguments.

## Preprocessor builtins

xcc's native preprocessor installs these predefined macros before processing
source files and before applying ordered command-line `-D` and `-U` actions.
That means `-U` can suppress a predefined macro, and a later `-D` can replace
it:

```sh
./xcc -U__XCC__ -D__XCC__=7 file.c
```

### `__XCC__`

Expands to the preprocessing number `1`.

Use it to detect xcc-specific behavior:

```c
#ifdef __XCC__
int using_xcc = 1;
#endif
```

### `__STDC__`

Expands to the preprocessing number `1`.

This identifies the implementation as a hosted C implementation for code that
checks standard predefined macros.

### `__DATE__`

Expands to a string literal containing the preprocessing start date in C's
traditional `"Mmm dd yyyy"` format.

The value is fixed when preprocessing begins. It honors `SOURCE_DATE_EPOCH` for
reproducible builds.

### `__TIME__`

Expands to a string literal containing the preprocessing start time in
`"hh:mm:ss"` format.

The value is fixed when preprocessing begins. It honors `SOURCE_DATE_EPOCH` for
reproducible builds.

### `__LINE__`

Expands to the current logical source line as a preprocessing number.

It is evaluated at each invocation location, so a macro that expands to
`__LINE__` reports the line where that macro is used. `#line` directives update
the logical line used for later expansions.

### `__FILE__`

Expands to the current logical source file name as a string literal.

It is evaluated at each invocation location. `#line` directives update the
logical file name used for later expansions.

## Header mapping

Use these headers for the portable names:

```c
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <stddef.h> /* offsetof */
#include <setjmp.h> /* jmp_buf, setjmp, longjmp */
#include <math.h>   /* HUGE_VAL */
```

Compile with `-Iinclude` so xcc finds its implementation headers before relying
on host-system headers with incompatible compiler assumptions.

## Unsupported builtin families

xcc does not currently implement GCC/Clang builtin families such as
`__builtin_alloca`, `__builtin_memcpy`, `__builtin_expect`,
`__builtin_types_compatible_p`, atomic builtins, synchronization builtins, or
vector builtins. Adding more builtins is listed as future work in the project
README.

