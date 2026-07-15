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
| `bsort.c`        | Bubble sort `int[5]`, return `a[0]`  | 3         |
| `star_bfs.c`     | BFS best path, decimal-coded nodes   | 15        |
| `matmul.c`       | 3x3 matrix multiply + weighted trace | 38        |
| `critter.c`      | Multi-species 8x8 world simulation   | 20        |
| `warnex.c`       | Warning demo (compiles; see stderr)  | 42        |

`critter.c` is a multi-species Cornell-style simulation on an 8x8 grid: three
critters (herbivore, carnivore, scavenger) with function-pointer brains, BFS
scent, collisions, struct-by-value snapshots, and a long checksum exit code.

`matmul.c` is a brace-initializer showcase: unsized multidimensional arrays with
both flat and nested initializers (`int A[][3]`, `int B[][3]`), a 1-D unsized
`long weight[]`, and scalar brace init (`int scale = {2}`). It multiplies two
3x3 matrices, takes a weighted trace, scales it, and returns the result mod 256.

`warnex.c` is a **deliberately sloppy** C89 program: it triggers many default
xcc warnings (implicit declarations, unprototyped calls, `char` overflow, old-
style definitions, bare `return` in a non-void function) but still links and
returns `42`. Compare stderr from xcc with a normal example like `gcd.c`.

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
diagnostics are printed instead. Files with `xcc-expect-warning` (currently
`warnex.c`) must compile and run successfully; warnings on stderr are expected
and shown, not treated as failure.
