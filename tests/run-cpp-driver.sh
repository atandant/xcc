#!/bin/sh
# SPDX-License-Identifier: MIT
# Driver-level preprocessing-only tests for -E.
set -u

XCC=./xcc
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

ok()
{
    echo "ok   -E $1"
    pass=$((pass + 1))
}

bad()
{
    echo "FAIL -E $1"
    fail=$((fail + 1))
}

cat > "$TMP/basic.c" <<'EOF'
#define VALUE 7
int value = VALUE;
EOF
if [ "$($XCC -E "$TMP/basic.c")" = "int value = 7 ;" ]; then
    ok "stdout"
else
    bad "stdout"
fi

if $XCC -E "$TMP/basic.c" -o "$TMP/basic.i" > "$TMP/stdout" &&
   [ ! -s "$TMP/stdout" ] &&
   [ "$(cat "$TMP/basic.i")" = "int value = 7 ;" ]; then
    ok "-o output"
else
    bad "-o output"
fi

cat > "$TMP/actions.c" <<'EOF'
#ifdef FEATURE
int selected = FEATURE;
#else
int selected = 0;
#endif
EOF
if [ "$($XCC -E -DFEATURE=9 -UFEATURE "$TMP/actions.c")" = \
     "int selected = 0 ;" ]; then
    ok "ordered -D/-U"
else
    bad "ordered -D/-U"
fi

cat > "$TMP/value.h" <<'EOF'
#pragma once
#define HEADER_VALUE 12
int from_header = HEADER_VALUE;
EOF
cat > "$TMP/include.c" <<'EOF'
#include "value.h"
#include "value.h"
int local = HEADER_VALUE;
EOF
if $XCC -E "$TMP/include.c" > "$TMP/include.i" &&
   [ "$(grep -c 'int from_header = 12 ;' "$TMP/include.i")" -eq 1 ] &&
   grep -Fx 'int local = 12 ;' "$TMP/include.i" > /dev/null; then
    ok "include expansion"
else
    bad "include expansion"
fi

cat > "$TMP/comments.c" <<'EOF'
int value = 1/* removed */+2;
EOF
if [ "$($XCC -E "$TMP/comments.c")" = "int value = 1 + 2 ;" ]; then
    ok "comment removal"
else
    bad "comment removal"
fi

cat > "$TMP/parser-independent.c" <<'EOF'
const double value = 1.5;
EOF
if [ "$($XCC -E "$TMP/parser-independent.c")" = \
     "const double value = 1.5 ;" ]; then
    ok "parser independence"
else
    bad "parser independence"
fi

if [ "$(printf '#define N 13\nint n=N;\n' | $XCC -E -)" = \
     "int n = 13 ;" ]; then
    ok "stdin"
else
    bad "stdin"
fi

printf '#error expected failure\nint n;\n' > "$TMP/error.c"
if ! $XCC -E "$TMP/error.c" > "$TMP/error.i" 2> "$TMP/error.err" &&
   grep -F '#error expected failure' "$TMP/error.err" > /dev/null; then
    ok "#error status"
else
    bad "#error status"
fi

cat > "$TMP/separation.c" <<'EOF'
#define PLUS +
int main(void) { int x; x = 1; return PLUS+x; }
EOF
if $XCC -E "$TMP/separation.c" > "$TMP/separation.i" &&
   gcc -std=c89 -x c "$TMP/separation.i" -o "$TMP/separation" &&
   "$TMP/separation"; then
    bad "safe token separation"
elif [ "$?" -eq 1 ]; then
    ok "safe token separation"
else
    bad "safe token separation"
fi

cat > "$TMP/roundtrip.c" <<'EOF'
#define RESULT (20 + 3)
int main(void) { return RESULT; }
EOF
if $XCC -E "$TMP/roundtrip.c" > "$TMP/roundtrip.i" &&
   $XCC "$TMP/roundtrip.i" -o "$TMP/roundtrip.s" &&
   gcc "$TMP/roundtrip.s" -o "$TMP/roundtrip"; then
    "$TMP/roundtrip"
    status=$?
    if [ "$status" -eq 23 ]; then ok "xcc round trip"; else bad "xcc round trip"; fi
else
    bad "xcc round trip"
fi

cat > "$TMP/gcc-pipe.c" <<'EOF'
#define RESULT (6 * 4)
int main(void) { return RESULT; }
EOF
if $XCC -E "$TMP/gcc-pipe.c" | gcc -std=c89 -x c - -o "$TMP/gcc-pipe"; then
    "$TMP/gcc-pipe"
    status=$?
    if [ "$status" -eq 24 ]; then ok "gcc pipe"; else bad "gcc pipe"; fi
else
    bad "gcc pipe"
fi

cat > "$TMP/message.c" <<'EOF'
#pragma message "stderr only"
int value = 1;
EOF
if $XCC -E "$TMP/message.c" > "$TMP/message.i" 2> "$TMP/message.err" &&
   [ "$(cat "$TMP/message.i")" = "int value = 1 ;" ] &&
   grep -F 'note: #pragma message: stderr only' "$TMP/message.err" > /dev/null; then
    ok "pragma message separation"
else
    bad "pragma message separation"
fi

echo "----"
echo "$pass -E driver tests passed, $fail failed"
[ "$fail" -eq 0 ]
