#!/bin/sh
# SPDX-License-Identifier: MIT
# Source-level preprocessing acceptance tests.
set -u

XCC=./xcc
DIR=tests/cpp
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

for src in "$DIR"/*.c; do
    file=$(basename "$src")
    expected=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_err=$(sed -n 's@^/[*] expect-error: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')

    if [ -n "$expected" ] && [ -n "$expected_err" ]; then
        echo "FAIL cpp/$file: both expect and expect-error"
        fail=$((fail + 1))
        continue
    fi
    if [ -n "$expected" ]; then
        if ! $XCC "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
            echo "FAIL cpp/$file (xcc error)"
            sed 's/^/      /' "$TMP/err"
            fail=$((fail + 1))
            continue
        fi
        if ! gcc "$TMP/out.s" -o "$TMP/out" 2> "$TMP/gccerr"; then
            echo "FAIL cpp/$file (assemble/link error)"
            sed 's/^/      /' "$TMP/gccerr"
            fail=$((fail + 1))
            continue
        fi
        "$TMP/out"
        got=$?
        if [ "$got" = "$expected" ]; then
            echo "ok   cpp/$file -> $got"
            pass=$((pass + 1))
        else
            echo "FAIL cpp/$file: expected $expected, got $got"
            fail=$((fail + 1))
        fi
    elif [ -n "$expected_err" ]; then
        if $XCC "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
            echo "FAIL cpp/$file: expected xcc error, got success"
            fail=$((fail + 1))
        elif grep ': error: ' "$TMP/err" | sed 's/^.*: error: //' |
             grep -Fx "$expected_err" > /dev/null; then
            echo "ok   cpp/$file -> xcc error"
            pass=$((pass + 1))
        else
            echo "FAIL cpp/$file: expected primary error '$expected_err'"
            sed 's/^/      /' "$TMP/err"
            fail=$((fail + 1))
        fi
    else
        echo "FAIL cpp/$file: missing expectation comment"
        fail=$((fail + 1))
    fi
done

echo "----"
echo "$pass preprocessor tests passed, $fail failed"
[ "$fail" -eq 0 ]
