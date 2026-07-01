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

## Status (v0.0.1.4)

The pipeline runs end to end (lex → parse → sema → codegen → `.s` → gcc) with
a typed semantic layer and 190+ acceptance tests.

| Supported | Not yet |
| --- | --- |
| `int`, `char`, `void`, pointers (`*`, `&`, dereference) | multi-dimensional arrays |
| 1D arrays, `[]` subscript, `ptr ± int`, param decay | pointer arithmetic beyond `±` / `[]` |
| typed declarations, parameters, returns | `sizeof`, brace initializers |
| casts `(type)expr`, `(void)expr` discard | `sizeof`, `long`, unsigned types |
| function calls (prototyped arg checking, void `*` conversions) | structs, unions, enums, `typedef` |
| `if` / `else`, `while`, `for`, blocks | preprocessor (`#include`, `#define`) |
| `+ - * / %`, unary `-`, comparisons | multiple translation units |
| pointer `==` / `!=` / ordering, truthiness, `p == 0` | `-W` CLI flags |
| warnings (`implicit` decl, unprototyped calls, `char` overflow, …) | |
| `file:line:col` diagnostics, carets, `note:` spans, color (`auto`) | |

> Locals use type-aware stack layout (`int` is 4 bytes, `char` is 1 byte).
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
                                      codegen ──▶ x86-64 .s ──▶ gcc ──▶ binary
```

The parser builds AST nodes and attaches parsed types to declarations. **sema**
owns type checking; codegen reads typed nodes and emits load/store/compare.

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
- later — preprocessor, structs, and enough to compile real projects

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
