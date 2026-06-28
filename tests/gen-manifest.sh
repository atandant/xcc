#!/bin/sh
# SPDX-License-Identifier: MIT
# Emit test manifest entries from /* expect: N */ comments in tests/cases/*.c.
set -u

DIR=${1:-tests/cases}

echo '# file|expected-exit-code'
for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    expected=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    if [ -z "$expected" ]; then
        echo "gen-manifest: missing /* expect: N */ in $src" >&2
        exit 1
    fi
    echo "$(basename "$src")|$expected"
done
