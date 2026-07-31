# xcc

A C89 compiler written from scratch, targeting x86-64 (System V ABI, Linux).
It emits AT&T-syntax assembly and hands assembling/linking off to `gcc`.

xcc is a `cc1`-style **compiler**, not a driver: normally one `.c` in and one
`.s` out; `-E` instead emits normalized, marker-free preprocessed C.

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

The pipeline runs end to end (preprocessing tokens → lex → parse → sema → lower → liveness →
regalloc → emit → `.s` → gcc) with a typed semantic layer and an extensive
acceptance test suite.

| Supported | Not yet |
| --- | --- |
| `char`, `short`, `int`, `long`, `void`, pointers (`*`, `&`, dereference) | `#line` |
| object/function-like `#define`, `#undef`, rescanning, `#`, `##`, and conditionals | multiple translation units |
| `#error` diagnostics | |
| `#pragma once`, `#pragma message`; unknown pragmas ignored | |
| marker-free preprocessing-only output with `-E` | |
| C89 predefined macros, `__XCC__`, and command-line `-D`/`-U` | |
| experimental `#include` with quoted/angle operands and `-I` search paths | |
| signed and unsigned integer types, promotions and usual arithmetic conversions | |
| parenthesized declarators (`int (*p)[3]`), casts (`(int (*)[3])`) | |
| N-dimensional arrays, `[]` subscript, `ptr ± int/long`, `ptr - ptr`, param decay | |
| typed declarations, parameters, returns | |
| **function pointers** (arrays, nested declarators, typedefs, casts, brace initialization, indirect calls and returns) | |
| **brace initializers** for scalar arrays, structs, and unions (partial init zero-fills) | |
| unsized `T a[] = {…}` / `T a[][N] = {…}` bound inference (flat or nested) | |
| scalar brace init `int x = {3};`, `int *p = {0};` | |
| **arrays of pointers** (`int *p[3]`) with brace init (`{0}`, `{&a, …}`) | |
| file-scope aggregate initialization (byte images, padding, and bit-fields) | |
| string initialization of local/file-scope `char[]` objects | |
| file-scope pointer constants (`&object`, member addresses, string literals and addends) | |
| casts `(type)expr`, `(void)expr` discard | |
| `sizeof expr`, `sizeof(type)` (compile-time fold, `unsigned long`) | |
| ordinary character constants (escapes and implementation-defined multi-character packing) | wide character and string literals |
| ordinary string literals in expressions (escapes, concatenation, embedded NUL) | floating types and literals |
| function calls (prototyped arg checking, void `*` conversions) | |
| `if` / `else`, `while`, `do ... while`, `for`, blocks and null statements | |
| `switch` / `case` / `default`, fallthrough, `break`, `continue` | |
| function-scoped labels and `goto` (forward and backward) | |
| prefix/postfix `++` and `--` on integer and pointer lvalues | |
| arithmetic, bitwise, shifts, comparisons, unary `+`/`-`/`~`, compound assignments, `&&`, `\|\|`, `!`, `?:`, comma | |
| pointer `==` / `!=` / ordering, truthiness, `p == 0` | |
| `U`/`u` and `L`/`l` integer suffixes (including `UL`/`LU`); `0x` hex literals (C89 typing) | `const` and `volatile` qualifiers |
| `int`/`long` promotions; `ptr - ptr` → `long` | |
| warnings (`implicit` decl, unprototyped calls, `char` overflow, …) | |
| `-W` / `-Wno-` / `-Wall` / `-Werror` CLI flags (`--help-warnings`) | |
| `file:line:col` diagnostics, carets, `note:` spans, color (`auto`) | |
| **`typedef`** (scoped names, declarator application) | |
| **`struct`** (tagged, forward-decl, layout), **`.` / `->`**, struct assign (`LIR_MEMCPY`), bitfields | |
| **`union`** (tagged, forward-decl, overlap layout), member access, union assign | |
| **`enum`** (tagged/anonymous, enumerator constants, enum variables as `int`) | |
| `struct S *` / `union U *` parameters and returns, struct/union by-value param/return | |
| file- and block-scope `extern`/`static`, block-scope `auto`/`register`, and `register` parameters | |
| | variadic functions and old-style function definitions with parameter declaration lists |

`typedef`, `extern`, and `static` are supported at file and block scope.

**Struct/union ABI:** passing and returning the compiler's naturally aligned,
integer/pointer-only records follows the SysV AMD64 INTEGER/MEMORY subset:
records up to 16 bytes use GPRs when the complete argument fits, while larger
records use sret/stack memory.

> Locals use type-aware stack layout (`char` 1 byte, `int` 4 bytes, `long` and
> pointers 8 bytes). On x86-64 Linux (SysV LP64), `long` is 64 bits — the same
> width as pointers.
>
> Plain `char` loads as an unsigned byte (0–255); use `unsigned char` when you
> need an explicitly unsigned type in expressions. Multi-character constants
> pack up to four decoded bytes from left to right (`'ab' == 0x6162`); this is
> xcc's implementation-defined C89 behavior.
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
 C source ──▶ preprocessing-token scanner ──▶ C-token adapter
                                                    │
                                                    ▼
                                            parser (bison) ──▶ AST
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
                                      liveness ──▶ regalloc
                                                   (range-aware weighted
                                                    linear scan)
                                                   │
                                                   ▼
                                      emit_x86 ──▶ x86-64 .s ──▶ gcc ──▶ binary
```

The native preprocessing-token input path is active, including C89 trigraphs,
line splicing, comment replacement, object-like macros, `#undef`, replacement
rescanning, and conditional inclusion. Function-like macros, argument prescan,
stringification, token pasting, the C89 predefined macros, `__XCC__`, and
command-line `-D`/`-U` are also implemented. Active `#error` directives emit
their unexpanded preprocessing-token messages. Unknown pragmas are ignored,
`#pragma once` suppresses repeated physical-file inclusions, and macro-expanded
`#pragma message` notes report build configuration on stderr. `-E` emits clean,
marker-free preprocessed C without invoking the parser. Includes and other
directives remain a work in progress; quoted and angle-bracket includes with
`-I` search paths are available experimentally. See
[`src/cpp/README.md`](src/cpp/README.md) for the detailed support matrix.

The parser builds AST nodes and attaches parsed types to declarations. **sema**
owns type checking. **lower** builds CFG-based LIR with virtual registers;
mem2reg promotes eligible locals into SSA form before CFG optimizations run.
**liveness** builds segmented live ranges, use/definition positions, and
loop-weighted spill costs. The range-aware linear-scan allocator reuses
registers across lifetime holes and assigns registers or spill slots; the
**emit_x86** prints AT&T assembly. Scalar locals and parameters that are not
address-taken may live in registers across loops; arrays and address-taken
locals stay on the stack.

Debug flags: `--xcc-dump-raw-lir` (unoptimized SSA LIR), `--xcc-dump-lir`
(optimized, phi-lowered LIR), `--xcc-dump-lir-alloc` (liveness, assigned
locations, and spill/frame metrics),
`--xcc-verify-lir`, and `--xcc-no-verify-lir`.

Warning flags: `-W<name>`, `-Wno-<name>`, `-Wall`, `-w`, `-Werror` — see
`--help-warnings` for the catalog. xcc `-Wall` enables warnings that default to
off; it is not gcc `-Wall`.

## Build & test

```sh
make            # requires bison + a C compiler
make test       # compile each tests/cases/*.c, run it, assert the exit code
./examples/build.sh
make clean
```

The generated parser source is produced into `build/` and is not committed.

## Roadmap

- `const` and `volatile` type qualifiers
- floating-point types, literals, operations, and ABI support
- variadic functions and old-style function definitions with parameter declarations
- function-pointer and nested-declarator conformance edge cases
- complete C89 preprocessor support
- multiple translation units and driver behavior
- broaden the SysV AMD64 ABI beyond the current integer/pointer record subset
- compile increasingly substantial C89 programs and libraries

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
