#!/bin/sh
# Acceptance harness: compile each case with xcc, assemble+link with gcc,
# run it, and compare the exit status against the manifest.
set -u

XCC=./xcc
DIR="tests/cases"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

while IFS='|' read -r file expected; do
    case "$file" in
        ''|\#*) continue ;;
    esac

    src="$DIR/$file"
    if ! "$XCC" "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
        echo "FAIL $file (xcc error)"
        sed 's/^/      /' "$TMP/err"
        fail=$((fail + 1))
        continue
    fi

    if ! gcc "$TMP/out.s" -o "$TMP/out" 2> "$TMP/gccerr"; then
        echo "FAIL $file (assemble/link error)"
        sed 's/^/      /' "$TMP/gccerr"
        fail=$((fail + 1))
        continue
    fi

    "$TMP/out"
    got=$?

    if [ "$got" = "$expected" ]; then
        echo "ok   $file -> $got"
        pass=$((pass + 1))
    else
        echo "FAIL $file: expected $expected, got $got"
        fail=$((fail + 1))
    fi
done < "$DIR/manifest.txt"

echo "----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
