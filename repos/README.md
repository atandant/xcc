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
