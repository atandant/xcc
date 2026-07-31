#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
JSMN="$ROOT/repos/jsmn"
BUILD="$ROOT/repos/build"

if [ ! -f "$JSMN/jsmn.h" ]; then
    "$ROOT/repos/fetch_jnsm.sh"
fi
if [ ! -x "$ROOT/xcc" ]; then
    make -C "$ROOT" xcc
fi

mkdir -p "$BUILD"
cat >"$BUILD/jsmn_smoke.c" <<'EOF'
#include "jsmn.h"

int main(void)
{
    const char json[] = "{\"ok\":true}";
    jsmn_parser parser;
    jsmntok_t tokens[3];
    int count;

    jsmn_init(&parser);
    count = jsmn_parse(&parser, json, sizeof(json) - 1, tokens, 3);
    return count == 3 && tokens[0].type == JSMN_OBJECT &&
           tokens[1].type == JSMN_STRING &&
           tokens[2].type == JSMN_PRIMITIVE ? 0 : 1;
}
EOF

build_one()
{
    name=$1
    shift
    "$ROOT/xcc" -I"$ROOT/include" -I"$JSMN" "$@" \
        "$BUILD/jsmn_smoke.c" -o "$BUILD/$name.s"
    "${CC:-cc}" "$BUILD/$name.s" -o "$BUILD/$name"
    "$BUILD/$name"
    echo "ok   $name"
}

build_one jsmn-default
build_one jsmn-strict -DJSMN_STRICT=1
build_one jsmn-parent-links -DJSMN_PARENT_LINKS=1
build_one jsmn-strict-parent-links -DJSMN_STRICT=1 -DJSMN_PARENT_LINKS=1
