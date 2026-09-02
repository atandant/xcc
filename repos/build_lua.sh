#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LUA="$ROOT/repos/lua"
BUILD="$ROOT/repos/build/lua"
TEST_BUILD="$BUILD/test"
HOST_CC=${CC:-cc}

if [ ! -f "$LUA/lua.h" ]; then
    "$ROOT/repos/fetch_lua.sh"
fi
if [ ! -x "$ROOT/xcc" ]; then
    make -C "$ROOT" xcc
fi

mkdir -p "$BUILD"

SOURCES="
lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes
lparser lstate lstring ltable ltm lundump lvm lzio
lauxlib lbaselib ldblib liolib lmathlib loslib ltablib lstrlib lutf8lib
loadlib lcorolib linit lua
"

OBJECTS=
for name in $SOURCES; do
    echo "CC   lua/$name.c"
    "$ROOT/xcc" -I"$ROOT/include" -I"$LUA" \
        -DLUA_USE_C89 -DLUA_NOBUILTIN -c "$LUA/$name.c" \
        -o "$BUILD/$name.o"
    OBJECTS="$OBJECTS $BUILD/$name.o"
done

# xcc targets non-PIE x86-64 code. Lua's math library is the only additional
# host library needed in portable C89 mode.
# shellcheck disable=SC2086
"$HOST_CC" -no-pie $OBJECTS -lm -o "$BUILD/lua"

"$BUILD/lua" -e \
    'local n=0; for i=1,100 do n=n+i end; assert(n==5050)'
echo "ok   lua-5.5.1"

mkdir -p "$TEST_BUILD"
TEST_OBJECTS=
for name in $SOURCES ltests; do
    echo "CC   lua/$name.c (internal tests)"
    "$ROOT/xcc" -I"$ROOT/include" -I"$LUA" \
        -DLUA_USE_C89 -DLUA_NOBUILTIN '-DLUA_USER_H="ltests.h"' \
        -c "$LUA/$name.c" -o "$TEST_BUILD/$name.o"
    TEST_OBJECTS="$TEST_OBJECTS $TEST_BUILD/$name.o"
done

# shellcheck disable=SC2086
"$HOST_CC" -no-pie $TEST_OBJECTS -lm -o "$TEST_BUILD/lua"

(cd "$LUA/testes" && "$TEST_BUILD/lua" -e '_port=true' all.lua)
echo "ok   lua-5.5.1-internal-portable-tests"
