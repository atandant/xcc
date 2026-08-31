#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UTHASH="$ROOT/repos/uthash"
BUILD="$ROOT/repos/build/uthash"

if [ ! -f "$UTHASH/src/uthash.h" ]; then
    "$ROOT/repos/fetch_uthash.sh"
fi
if [ ! -x "$ROOT/xcc" ]; then
    make -C "$ROOT" xcc
fi

mkdir -p "$BUILD"

build_one()
{
    name=$1
    "$ROOT/xcc" -DNO_DECLTYPE=1 -I"$ROOT/include" -I"$UTHASH/src" \
        "$UTHASH/tests/$name.c" -o "$BUILD/$name.s"
    "${CC:-cc}" "$BUILD/$name.s" -o "$BUILD/$name"
    "$BUILD/$name" >"$BUILD/$name.out"
    cmp "$UTHASH/tests/$name.ans" "$BUILD/$name.out"
    echo "ok   uthash-$name"
}

# Integer keys and insertion/iteration, compound keys, and pointer keys with
# find/delete respectively. test21 and test57 also exercise anonymous structs.
build_one test1
build_one test21
build_one test57
