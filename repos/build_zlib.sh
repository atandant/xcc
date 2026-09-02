#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ZLIB="$ROOT/repos/zlib"
BUILD="$ROOT/repos/build/zlib"

if [ ! -f "$ZLIB/zlib.h" ]; then
    "$ROOT/repos/fetch_zlib.sh"
fi
if [ ! -x "$ROOT/xcc" ]; then
    make -C "$ROOT" xcc
fi

mkdir -p "$BUILD"

build_test()
{
    name=$1
    "$ROOT/xcc" -DHAVE_UNISTD_H -I"$ROOT/include" -I"$ZLIB" \
        "$ZLIB/adler32.c" \
        "$ZLIB/crc32.c" \
        "$ZLIB/deflate.c" \
        "$ZLIB/gzclose.c" \
        "$ZLIB/gzlib.c" \
        "$ZLIB/gzread.c" \
        "$ZLIB/gzwrite.c" \
        "$ZLIB/infback.c" \
        "$ZLIB/inffast.c" \
        "$ZLIB/inflate.c" \
        "$ZLIB/inftrees.c" \
        "$ZLIB/trees.c" \
        "$ZLIB/zutil.c" \
        "$ZLIB/compress.c" \
        "$ZLIB/uncompr.c" \
        "$ZLIB/test/$name.c" \
        -o "$BUILD/zlib-$name"
}

build_test example
build_test minigzip
build_test infcover

TESTFILE="$BUILD/example.gz"
PLAIN="$BUILD/minigzip.txt"
COMPRESSED="$BUILD/minigzip.gz"
ROUNDTRIP="$BUILD/minigzip.out"
trap 'rm -f "$TESTFILE" "$PLAIN" "$COMPRESSED" "$ROUNDTRIP"' EXIT

"$BUILD/zlib-example" "$TESTFILE"
echo "ok   zlib-example"

printf 'hello world\n' >"$PLAIN"
"$BUILD/zlib-minigzip" <"$PLAIN" >"$COMPRESSED"
"$BUILD/zlib-minigzip" -d <"$COMPRESSED" >"$ROUNDTRIP"
cmp "$PLAIN" "$ROUNDTRIP"
echo "ok   zlib-minigzip"

"$BUILD/zlib-infcover" >"$BUILD/infcover.out" 2>&1
echo "ok   zlib-infcover"
