#!/bin/sh
# SPDX-License-Identifier: MIT
# Emit test manifest entries from expectation comments in tests/cases/*.c.
set -u

DIR=${1:-tests/cases}

echo '# kind|file|expected-code|expected-stderr-substring'
for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    expect_run=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expect_error=$(sed -n 's@^/[*] expect-error: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')

    if [ -n "$expect_run" ] && [ -n "$expect_error" ]; then
        echo "gen-manifest: both expect and expect-error in $src" >&2
        exit 1
    fi
    if [ -n "$expect_run" ]; then
        echo "run|$(basename "$src")|$expect_run|"
    elif [ -n "$expect_error" ]; then
        echo "xcc-error|$(basename "$src")|1|$expect_error"
    else
        echo "gen-manifest: missing /* expect: N */ or /* expect-error: text */ in $src" >&2
        exit 1
    fi
done
