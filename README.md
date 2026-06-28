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

## Status (v0.0.1)

This is the walking skeleton: the whole pipeline runs end to end
(lex → parse → sema → codegen → `.s` → gcc), with a real test harness.

| Supported                                   | Not yet                                  |
| ------------------------------------------- | ---------------------------------------- |
| `int main(void) { ... }`                    | function parameters / calls / ABI        |
| `int` locals (`int a;`, `int a = 5;`)       | other types: `char`, pointers, arrays…   |
| `return`, assignment, expression statements | `while` / `for`                          |
| `if` / `else`, compound blocks             | the preprocessor (`#include`, `#define`) |
| `+ - * / %`, unary `-`, parentheses         | strict C89 mode                          |
| `== != < <= > >=`                           | multiple functions / translation units   |
| `file:line:col` diagnostics to stderr       | optimizations                            |

> Known deviation: `int` is computed in 64-bit registers for now (correct
> 32-bit width arrives with the type system). Only the low 8 bits reach the
> process exit status, so it is invisible to the current tests.

## How it works

```
 C source ──▶ lexer (flex) ──▶ parser (bison) ──▶ AST
                                                   │
                          arena-allocated, thin    │
                          grammar actions only     ▼
                                            sema (resolve locals,
                                                  check errors)
                                                   │
                                                   ▼
                                      codegen ──▶ x86-64 .s ──▶ gcc ──▶ binary
```

The parser only builds AST nodes; all checking lives in a separate `sema`
pass, and codegen walks the AST straight to assembly (no IR yet).

## Build & test

```sh
make        # requires bison + flex + a C compiler
make test   # compile each tests/cases/*.c, run it, assert the exit code
make clean
```

Generated parser/lexer sources are produced into `build/` and are not
committed.

## Roadmap

- `0.0.1.1` — remaining control flow (`while`, `for`)
- `0.0.1.2` — functions, parameters, calls (System V ABI)
- `0.0.1.3` — pointers, `char`, arrays, real `int` width / type system
- later — the preprocessor, structs, and enough to compile real projects

## License

MIT — see [LICENSE](LICENSE). Build-time tool credits in
[THIRD_PARTY.md](THIRD_PARTY.md).
