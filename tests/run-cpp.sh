#!/bin/sh
# SPDX-License-Identifier: MIT
# Source-level preprocessing acceptance tests.
set -u

XCC=./xcc
DIR=tests/cpp
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

IDENTITY_TMP="$TMP/pragma-identity"
mkdir -p "$IDENTITY_TMP"
cp "$DIR/fixtures/include/pragma/identity.h" \
   "$IDENTITY_TMP/identity-original.h"
ln -s identity-original.h "$IDENTITY_TMP/identity-symlink.h"
ln "$IDENTITY_TMP/identity-original.h" "$IDENTITY_TMP/identity-hardlink.h"

pass=0
fail=0

for src in "$DIR"/*.c; do
    file=$(basename "$src")
    expected=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_err=$(sed -n 's@^/[*] expect-error: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_location=$(sed -n 's@^/[*] expect-location: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_note=$(sed -n 's@^/[*] expect-note: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_warning=$(sed -n 's@^/[*] expect-warning: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_no_note=$(sed -n 's@^/[*] expect-no-note: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_no_warning=$(sed -n 's@^/[*] expect-no-warning: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_include=$(sed -n 's@^/[*] expect-include: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expected_include_count=$(sed -n 's@^/[*] expect-include-count: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    cpp_flags=$(sed -n 's@^/[*] cpp-flags: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    source_date_epoch=$(sed -n 's@^/[*] source-date-epoch: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    pragma_identity_fixture=$(sed -n 's@^/[*] pragma-identity-fixture [*]/$@yes@p' "$src" | sed -n '1p')
    set -f
    set -- $cpp_flags
    set +f
    if [ -n "$pragma_identity_fixture" ]; then
        set -- "$@" "-I$IDENTITY_TMP"
    fi

    if [ -n "$expected" ] && [ -n "$expected_err" ]; then
        echo "FAIL cpp/$file: both expect and expect-error"
        fail=$((fail + 1))
        continue
    fi
    if [ -n "$expected" ]; then
        if ! SOURCE_DATE_EPOCH=$source_date_epoch \
             $XCC -S "$@" "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
            echo "FAIL cpp/$file (xcc error)"
            sed 's/^/      /' "$TMP/err"
            fail=$((fail + 1))
            continue
        fi
        if [ -n "$expected_note" ] &&
           ! grep ': note: ' "$TMP/err" | sed 's/^.*: note: //' |
             grep -Fx "$expected_note" > /dev/null; then
            echo "FAIL cpp/$file: expected note '$expected_note'"
            sed 's/^/      /' "$TMP/err"
            fail=$((fail + 1))
            continue
        fi
        if [ -n "$expected_warning" ] &&
           ! grep ': warning: ' "$TMP/err" | sed 's/^.*: warning: //' |
             grep -Fx "$expected_warning" > /dev/null; then
            echo "FAIL cpp/$file: expected warning '$expected_warning'"
            sed 's/^/      /' "$TMP/err"
            fail=$((fail + 1))
            continue
        fi
        if [ -n "$expected_no_note" ] &&
           grep ': note: ' "$TMP/err" | sed 's/^.*: note: //' |
             grep -Fx "$expected_no_note" > /dev/null; then
            echo "FAIL cpp/$file: unexpected note '$expected_no_note'"
            fail=$((fail + 1))
            continue
        fi
        if [ -n "$expected_no_warning" ] &&
           grep ': warning: ' "$TMP/err" | sed 's/^.*: warning: //' |
             grep -Fx "$expected_no_warning" > /dev/null; then
            echo "FAIL cpp/$file: unexpected warning '$expected_no_warning'"
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
        if SOURCE_DATE_EPOCH=$source_date_epoch \
           $XCC -S "$@" "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
            echo "FAIL cpp/$file: expected xcc error, got success"
            fail=$((fail + 1))
        elif grep ': error: ' "$TMP/err" | sed 's/^.*: error: //' |
             grep -Fx "$expected_err" > /dev/null; then
            include_count=$(grep -c '^In file included from ' "$TMP/err")
            if [ -n "$expected_location" ] &&
               ! grep -F "$expected_location" "$TMP/err" > /dev/null; then
                echo "FAIL cpp/$file: expected location '$expected_location'"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            elif [ -n "$expected_include" ] &&
               ! grep -Fx "$expected_include" "$TMP/err" > /dev/null; then
                echo "FAIL cpp/$file: expected include trace '$expected_include'"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            elif [ -n "$expected_include_count" ] &&
                 [ "$include_count" -ne "$expected_include_count" ]; then
                echo "FAIL cpp/$file: expected $expected_include_count include trace lines, got $include_count"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            else
                echo "ok   cpp/$file -> xcc error"
                pass=$((pass + 1))
            fi
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
