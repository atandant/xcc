# Third-party compiler checks

## jsmn

From the xcc repository root:

```sh
./repos/fetch_jnsm.sh
./repos/build_jsmn.sh
```

The build script:

1. builds `xcc` if necessary;
2. compiles a functional jsmn program with xcc;
3. assembles and links xcc's output with `${CC:-cc}`; and
4. runs all four combinations of `JSMN_STRICT` and `JSMN_PARENT_LINKS`.

Generated sources, assembly, and executables are placed in `repos/build/`.
The fetched checkout and build output are intentionally ignored by git.

For a direct compilation, xcc's implementation headers and the fetched jsmn
header must both be on the include path:

```sh
./xcc -Iinclude -Irepos/jsmn program.c -o program.s
cc program.s -o program
```

## uthash

From the xcc repository root:

```sh
./repos/fetch_uthash.sh
./repos/build_uthash.sh
```

The pinned uthash 2.3.0 checks cover integer, compound, and pointer keys, along
with insertion, lookup, iteration, and deletion. The script defines
`NO_DECLTYPE=1`, uthash's upstream portability mode for compilers without GNU
`__typeof__`; no patch to the downloaded project is required. The compound-key
and pointer-key checks also exercise xcc's anonymous struct typedef support.

## zlib

From the xcc repository root:

```sh
./repos/fetch_zlib.sh
./repos/build_zlib.sh
```

The pinned zlib 1.3.1 build compiles all fifteen library translation units and
the unmodified upstream `example.c`, `minigzip.c`, and `infcover.c` tests
through xcc's driver. These cover compression, checksums, inflate error paths,
multi-translation-unit linking, gzip pipe round-trips, and the `gz*` file API,
including formatted output, reading, writing, seeking, and closing. The build
uses xcc's x86-64 Linux POSIX compatibility headers rather than host headers.
