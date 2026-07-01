# xcc examples

Small programs for xcc **0.0.1.3**.

## Runnable programs

Each `main` returns a value you can inspect with `echo $?` after running the
binary.

| File             | What it does                         | Exit code |
| ---------------- | ------------------------------------ | --------- |
| `primes.c`       | Count primes ≤ 100                   | 25        |
| `fibonacci.c`    | `fib(10)`                            | 55        |
| `gcd.c`          | `gcd(48, 18)`                        | 6         |
| `pointer_swap.c` | Swap via `int *`, return `x + y`     | 30        |

These use loops, functions, and (in `pointer_swap.c`) address-of, dereference,
and pointer parameters. They do **not** need arrays or pointer arithmetic.

## Error demonstration (`errnop.c`)

`errnop.c` is a **deliberately broken** pointer program. xcc should reject it
with multiple errors and notes: carets, conflicting-type spans, incompatible
assignments and comparisons, void `*` dereference, bad calls, redeclaration, and
non-lvalue address-of.

```sh
./xcc examples/errnop.c          # diagnostic tour on stderr
./examples/build.sh              # treats errnop as an expected-failure demo
```

## Build and run

From the repository root (after `make`):

```sh
./examples/build.sh
```

Or one runnable file by hand:

```sh
./xcc examples/primes.c -o /tmp/primes.s
gcc /tmp/primes.s -o /tmp/primes
/tmp/primes
echo $?    # -> 25
```

The build script compiles every runnable `examples/*.c` file, links with `gcc`,
runs each binary, and prints its exit code. Files containing `xcc-expect-error`
in a comment (currently `errnop.c`) are compiled expecting failure; their
diagnostics are printed instead.
