#!/bin/sh
# SPDX-License-Identifier: MIT
# Acceptance harness: regenerate the manifest, then run executable and
# diagnostic cases against it.
set -u

XCC=./xcc
DIR="tests/cases"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MANIFEST="$DIR/manifest.txt"
./tests/gen-manifest.sh "$DIR" > "$MANIFEST" || exit 1

pass=0
fail=0

while IFS='|' read -r kind file expected expected_err; do
    case "$kind" in
        ''|\#*) continue ;;
    esac

    src="$DIR/$file"
    case "$kind" in
        run)
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
            ;;
        xcc-error)
            if "$XCC" "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
                echo "FAIL $file: expected xcc error, got success"
                fail=$((fail + 1))
                continue
            fi

            if grep -F "$expected_err" "$TMP/err" > /dev/null; then
                echo "ok   $file -> xcc error"
                pass=$((pass + 1))
            else
                echo "FAIL $file: expected stderr containing '$expected_err'"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            fi
            ;;
        *)
            echo "FAIL manifest: unknown test kind '$kind' for $file"
            fail=$((fail + 1))
            ;;
    esac
done < "$MANIFEST"

echo "----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
