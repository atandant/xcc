# xcc examples

Small programs that compile with xcc **0.0.1.3**. Each `main` returns a value
you can inspect with `echo $?` after running the binary.

| File             | What it does                         | Exit code |
| ---------------- | ------------------------------------ | --------- |
| `primes.c`       | Count primes ≤ 100                   | 25        |
| `fibonacci.c`    | `fib(10)`                            | 55        |
| `gcd.c`          | `gcd(48, 18)`                        | 6         |
| `pointer_swap.c` | Swap via `int *`, return `x + y`     | 30        |

These programs use loops, functions, and (in `pointer_swap.c`) address-of,
dereference, and pointer parameters. They do **not** need arrays or pointer
arithmetic.

## Build and run

From the repository root (after `make`):

```sh
./examples/build.sh
```

Or one file by hand:

```sh
./xcc examples/primes.c -o /tmp/primes.s
gcc /tmp/primes.s -o /tmp/primes
/tmp/primes
echo $?    # -> 25
```

The build script compiles every `examples/*.c` file, links with `gcc`, runs
each binary, and prints its exit code.
