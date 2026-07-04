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

## Status (v0.0.1.6)

The pipeline runs end to end (lex → parse → sema → lower → liveness →
regalloc → emit → `.s` → gcc) with a typed semantic layer and 340+ acceptance
tests.

| Supported | Not yet |
| --- | --- |
| `int`, `char`, `long`, `void`, pointers (`*`, `&`, dereference) | parenthesized declarators (`int (*p)[3]`) |
| N-dimensional arrays, `[]` subscript, `ptr ± int/long`, param decay | pointer arithmetic beyond `±` / `[]` |
| typed declarations, parameters, returns | `sizeof`, brace initializers |
| casts `(type)expr`, `(void)expr` discard | unsigned types, `short`|
| function calls (prototyped arg checking, void `*` conversions) | structs, unions, enums, `typedef` |
| `if` / `else`, `while`, `for`, blocks | preprocessor (`#include`, `#define`) |
| `+ - * / %`, unary `-`, comparisons | multiple translation units |
| pointer `==` / `!=` / ordering, truthiness, `p == 0` | `-W` CLI flags |
| `L`/`l` literal suffixes; unsuffixed decimals → `int` or `long` | |
| `int`/`long` promotions; `ptr - ptr` → `long` | |
| warnings (`implicit` decl, unprototyped calls, `char` overflow, …) | |
| `file:line:col` diagnostics, carets, `note:` spans, color (`auto`) | |

> Locals use type-aware stack layout (`char` 1 byte, `int` 4 bytes, `long` and
> pointers 8 bytes). On x86-64 Linux (SysV LP64), `long` is 64 bits — the same
> width as pointers.
>
> `auto` arrays are not zero-initialized (faithful C89).

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
- later — preprocessor, structs, and enough to compile real projects

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
