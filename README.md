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

## Status (v0.0.1.7)

The pipeline runs end to end (lex → parse → sema → lower → liveness →
regalloc → emit → `.s` → gcc) with a typed semantic layer and 420+ acceptance
tests.

| Supported | Not yet |
| --- | --- |
| `int`, `char`, `long`, `void`, pointers (`*`, `&`, dereference) | structs, unions, enums, `typedef` |
| `unsigned` (`char`/`short`/`int`/`long`), usual arithmetic conversions | function pointers |
| parenthesized declarators (`int (*p)[3]`), casts (`(int (*)[3])`) | preprocessor (`#include`, `#define`) |
| N-dimensional arrays, `[]` subscript, `ptr ± int/long`, `ptr - ptr`, param decay | multiple translation units |
| typed declarations, parameters, returns | `-W` CLI flags |
| **brace initializers** for any scalar array — `char`/`short`/`int`/`long`, signed & unsigned (1D and multidim, flat or nested; trailing comma OK) | |
| unsized `T a[] = {…}` / `T a[][N] = {…}` bound inference (flat or nested) | |
| scalar brace init `int x = {3};`, `int *p = {0};` | |
| **arrays of pointers** (`int *p[3]`) with brace init (`{0}`, `{&a, …}`) | |
| casts `(type)expr`, `(void)expr` discard | |
| `sizeof expr`, `sizeof(type)` (compile-time fold, `unsigned long`) | |
| function calls (prototyped arg checking, void `*` conversions) | |
| `if` / `else`, `while`, `for`, blocks | |
| `+ - * / %`, unary `-`, comparisons (signed and unsigned) | |
| pointer `==` / `!=` / ordering, truthiness, `p == 0` | |
| `L`/`l` literal suffixes; `0x` hex literals (C89 typing) | |
| `int`/`long` promotions; `ptr - ptr` → `long` | |
| warnings (`implicit` decl, unprototyped calls, `char` overflow, …) | |
| `file:line:col` diagnostics, carets, `note:` spans, color (`auto`) | |

**Brace initializer scope (deferred):** file-scope/static aggregate init;
string literals for `char[]`; nested brace init for pointer-to-array types;
`{…}` bulk-zero via `memset` (uses per-element stores today).

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
                                      lower ──▶ LIR
                                                   │
                                                   ▼
                                      liveness ──▶ regalloc (linear scan)
                                                   │
                                                   ▼
                                      emit_x86 ──▶ x86-64 .s ──▶ gcc ──▶ binary
```

The parser builds AST nodes and attaches parsed types to declarations. **sema**
owns type checking. **lower** builds LIR with virtual registers; **liveness**
and **regalloc** assign registers or spill slots; **emit_x86** prints AT&T
assembly. Scalar locals and parameters that are not address-taken may live in
registers across loops; arrays and address-taken locals stay on the stack.

Debug flags: `--emit-lir` (LIR before allocation), `--emit-lir-alloc` (after).

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

- `0.0.1.1` — `while`, `for` ✓
- `0.0.1.2` — functions, parameters, calls (System V ABI) ✓
- `0.0.1.3` — type system, pointers, `char`, `void *` ✓
- `0.0.1.4` — 1D arrays, subscript, pointer arithmetic, layout fix ✓
- `0.0.1.5` — `long` (LP64 64-bit), literal suffixes, promotions, `ptr - ptr` ✓
- `0.0.1.6` — LIR lowering, liveness, linear-scan register allocation ✓
- `0.0.1.6` — `sizeof` (compile-time fold, abstract declarator types) ✓
- later — preprocessor, structs, and enough to compile real projects

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
