# xcc

A C89 compiler written from scratch, targeting x86-64 (System V ABI, Linux).
It emits AT&T-syntax assembly and hands assembling/linking off to `gcc`.

xcc is a `cc1`-style **compiler**, not a driver: one `.c` in, one `.s` out.

## Quickstart

```sh
make
echo 'int main(void){ return 2*(3+4)-1; }' | ./xcc | gcc -xassembler - -o a && ./a
echo $?   # -> 13
```

Or against files:

```sh
./xcc program.c -o program.s
gcc program.s -o program
```

## Examples

See [`examples/`](examples/) for small programs (prime sieve count, Fibonacci,
GCD, pointer swap). Build and run them all:

```sh
make
./examples/build.sh
```

## Status

The pipeline runs end to end (lex → parse → sema → lower → liveness →
regalloc → emit → `.s` → gcc) with a typed semantic layer and 696 test checks.

| Supported | Not yet |
| --- | --- |
| `char`, `short`, `int`, `long`, `void`, pointers (`*`, `&`, dereference) | preprocessor (`#include`, `#define`) |
| signed and unsigned integer types, promotions and usual arithmetic conversions | multiple translation units |
| parenthesized declarators (`int (*p)[3]`), casts (`(int (*)[3])`) | |
| N-dimensional arrays, `[]` subscript, `ptr ± int/long`, `ptr - ptr`, param decay | |
| typed declarations, parameters, returns | |
| **function pointers** (declarators, typedef, casts, indirect calls) | |
| **brace initializers** for scalar arrays and structs (partial init zero-fills) | |
| unsized `T a[] = {…}` / `T a[][N] = {…}` bound inference (flat or nested) | |
| scalar brace init `int x = {3};`, `int *p = {0};` | |
| **arrays of pointers** (`int *p[3]`) with brace init (`{0}`, `{&a, …}`) | |
| casts `(type)expr`, `(void)expr` discard | |
| `sizeof expr`, `sizeof(type)` (compile-time fold, `unsigned long`) | |
| function calls (prototyped arg checking, void `*` conversions) | |
| `if` / `else`, `while`, `do ... while`, `for`, blocks and null statements | |
| `switch` / `case` / `default`, fallthrough, `break`, `continue` | |
| function-scoped labels and `goto` (forward and backward) | |
| arithmetic, bitwise, shifts, comparisons, `&&`, `\|\|`, `!`, `?:`, comma | |
| pointer `==` / `!=` / ordering, truthiness, `p == 0` | |
| `L`/`l` literal suffixes; `0x` hex literals (C89 typing) | |
| `int`/`long` promotions; `ptr - ptr` → `long` | |
| warnings (`implicit` decl, unprototyped calls, `char` overflow, …) | |
| `-W` / `-Wno-` / `-Wall` / `-Werror` CLI flags (`--help-warnings`) | |
| `file:line:col` diagnostics, carets, `note:` spans, color (`auto`) | |
| **`typedef`** (scoped names, declarator application) | |
| **`struct`** (tagged, forward-decl, layout), **`.` / `->`**, struct assign (`LIR_MEMCPY`), bitfields | |
| **`union`** (tagged, forward-decl, overlap layout), member access, union assign | |
| **`enum`** (tagged/anonymous, enumerator constants, enum variables as `int`) | |
| `struct S *` / `union U *` parameters and returns, struct/union by-value param/return | |

**Brace initializer scope (deferred):** file-scope/static aggregate init;
string literals for `char[]`; nested brace init for pointer-to-array types.

**Function pointers (deferred):** arrays of function pointers (`int (*a[3])(int)`);
brace initializers for fnptrs; nested declarators such as
`int (*(*x)(int))(char)` (use typedefs); file-scope definitions whose return
type is a function pointer without a typedef (e.g. `int (*f(void))(int) { … }`).

**Struct/union ABI:** passing and returning the compiler's naturally aligned,
integer/pointer-only records follows the SysV AMD64 INTEGER/MEMORY subset:
records up to 16 bytes use GPRs when the complete argument fits, while larger
records use sret/stack memory.

> Locals use type-aware stack layout (`char` 1 byte, `int` 4 bytes, `long` and
> pointers 8 bytes). On x86-64 Linux (SysV LP64), `long` is 64 bits — the same
> width as pointers.
>
> Plain `char` loads as an unsigned byte (0–255); use `unsigned char` when you
> need an unsigned type in expressions. `U`/`u` literal suffixes are not supported
> (C99); use `0x` constants or casts instead.
>
> `sizeof` is evaluated at compile time in sema (no runtime codegen). Array
> operands do not decay; function, `void`, and incomplete types are rejected.
> The operand of `sizeof expr` is not evaluated.
>
> Brace initializers pad unspecified elements with zero (`{0}` zero-fills the
> whole array). Partial lists and excess-element errors follow C89 aggregate rules.
> Init element expressions are lowered normally so `cosfold`/`cosprop` can fold them.
>
> `auto` arrays without an initializer are not zero-initialized (faithful C89).

## How it works

```
 C source ──▶ lexer (flex) ──▶ parser (bison) ──▶ AST
                                                   │
                          arena-allocated, thin    │
                          grammar actions only     ▼
                                            sema (types, lvalues,
                                                  function table)
                                                   │
                                                   ▼
                                      lower ──▶ CFG LIR
                                                   │
                                                   ▼
                                  mem2reg / SSA optimizations
                                  (constant folding, DCE, LICM,
                                   algebraic and strength reduction)
                                                   │
                                                   ▼
                                      liveness ──▶ regalloc (linear scan)
                                                   │
                                                   ▼
                                      emit_x86 ──▶ x86-64 .s ──▶ gcc ──▶ binary
```

The parser builds AST nodes and attaches parsed types to declarations. **sema**
owns type checking. **lower** builds CFG-based LIR with virtual registers;
mem2reg promotes eligible locals into SSA form before CFG optimizations run.
**liveness** and **regalloc** then assign registers or spill slots, and
**emit_x86** prints AT&T assembly. Scalar locals and parameters that are not
address-taken may live in registers across loops; arrays and address-taken
locals stay on the stack.

Debug flags: `--xcc-dump-raw-lir` (unoptimized SSA LIR), `--xcc-dump-lir`
(optimized, phi-lowered LIR), `--xcc-dump-lir-alloc` (live intervals),
`--xcc-verify-lir`, and `--xcc-no-verify-lir`.

Warning flags: `-W<name>`, `-Wno-<name>`, `-Wall`, `-w`, `-Werror` — see
`--help-warnings` for the catalog. xcc `-Wall` enables warnings that default to
off; it is not gcc `-Wall`.

## Build & test

```sh
make            # requires bison + flex + a C compiler
make test       # compile each tests/cases/*.c, run it, assert the exit code
./examples/build.sh
make clean
```

Generated parser/lexer sources are produced into `build/` and are not
committed.

## Roadmap

- string literals and file-scope/static objects
- remaining nested declarator corner cases
- preprocessor support
- multiple translation units and driver behavior
- broaden the SysV AMD64 ABI beyond the current integer/pointer record subset
- compile increasingly substantial C89 programs and libraries

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
