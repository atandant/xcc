#!/bin/sh
# SPDX-License-Identifier: MIT
# Compile and run every example with xcc.
# Files marked with "xcc-expect-error" are expected to fail xcc and only
# print their diagnostics (see examples/errnop.c).
# Files marked with "xcc-expect-warning" compile and run, but stderr is
# expected to carry warnings (see examples/warnex.c).
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
demo=0
warn_demo=0

is_error_demo() {
    grep -q 'xcc-expect-error' "$1"
}

is_warning_demo() {
    grep -q 'xcc-expect-warning' "$1"
}

for src in "$EXDIR"/*.c; do
    [ -e "$src" ] || continue
    name=$(basename "$src" .c)

    if is_error_demo "$src"; then
        if "$XCC" "$src" -o "$OUTDIR/$name" 2> "$OUTDIR/$name.err"; then
            echo "FAIL $name (expected xcc compile error)"
            fail=$((fail + 1))
            continue
        fi
        echo "ok   $name -> expected compile failure ($(
            grep -c ': error: ' "$OUTDIR/$name.err" || true
        ) errors); diagnostics:"
        sed 's/^/  /' "$OUTDIR/$name.err"
        demo=$((demo + 1))
        continue
    fi

    if ! "$XCC" "$src" -o "$OUTDIR/$name" 2> "$OUTDIR/$name.err"; then
        echo "FAIL $name (xcc)"
        sed 's/^/  /' "$OUTDIR/$name.err"
        fail=$((fail + 1))
        continue
    fi

    if is_warning_demo "$src"; then
        nwarn=$(grep -c ': warning: ' "$OUTDIR/$name.err" || true)
        if [ "$nwarn" -eq 0 ]; then
            echo "FAIL $name (expected xcc warnings, got none)"
            fail=$((fail + 1))
            continue
        fi
        if grep -q ': error: ' "$OUTDIR/$name.err"; then
            echo "FAIL $name (expected warnings only, got errors)"
            sed 's/^/  /' "$OUTDIR/$name.err"
            fail=$((fail + 1))
            continue
        fi
    fi

    if "$OUTDIR/$name"; then
        code=0
    else
        code=$?
    fi
    if is_warning_demo "$src"; then
        nwarn=$(grep -c ': warning: ' "$OUTDIR/$name.err" || true)
        echo "ok   $name -> exit $code ($nwarn warnings); diagnostics:"
        sed 's/^/  /' "$OUTDIR/$name.err"
        warn_demo=$((warn_demo + 1))
    else
        echo "ok   $name -> exit $code"
    fi
    pass=$((pass + 1))
done

echo "----"
echo "$pass runnable, $demo error demo(s), $warn_demo warning demo(s), $fail failed"
[ "$fail" -eq 0 ]
