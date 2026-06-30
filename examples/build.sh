#!/bin/sh
# SPDX-License-Identifier: MIT
# Compile and run every example with xcc + gcc.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
XCC="$ROOT/xcc"
EXDIR="$ROOT/examples"
OUTDIR=$(mktemp -d)
trap 'rm -rf "$OUTDIR"' EXIT

if [ ! -x "$XCC" ]; then
    echo "examples/build.sh: run 'make' in $ROOT first" >&2
    exit 1
fi

pass=0
fail=0

for src in "$EXDIR"/*.c; do
    [ -e "$src" ] || continue
    name=$(basename "$src" .c)

    if ! "$XCC" "$src" -o "$OUTDIR/$name.s" 2> "$OUTDIR/$name.err"; then
        echo "FAIL $name (xcc)"
        sed 's/^/  /' "$OUTDIR/$name.err"
        fail=$((fail + 1))
        continue
    fi

    if ! gcc "$OUTDIR/$name.s" -o "$OUTDIR/$name" 2> "$OUTDIR/$name.gccerr"; then
        echo "FAIL $name (gcc)"
        sed 's/^/  /' "$OUTDIR/$name.gccerr"
        fail=$((fail + 1))
        continue
    fi

    if "$OUTDIR/$name"; then
        code=0
    else
        code=$?
    fi
    echo "ok   $name -> exit $code"
    pass=$((pass + 1))
done

echo "----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
