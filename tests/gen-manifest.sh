#!/bin/sh
# SPDX-License-Identifier: MIT
# Emit test manifest entries from expectation comments in tests/cases/*.c.
set -u

DIR=${1:-tests/cases}
ABI_DIR=${2:-tests/abi}

echo '# kind|file|expected-code|expected-stderr-substring|xcc-args|link-args'
for src in "$DIR"/*.c; do
    [ -e "$src" ] || continue
    expect_run=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expect_stdout=$(sed -n 's@^/[*] expect-stdout: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expect_error=$(sed -n 's@^/[*] expect-error: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    expect_warning=$(sed -n 's@^/[*] expect-warning: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    xcc_args=$(sed -n 's@^/[*] xcc-args: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')
    link_args=$(sed -n 's@^/[*] link-args: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')

    expectation_count=0
    [ -n "$expect_run" ] && expectation_count=$((expectation_count + 1))
    [ -n "$expect_stdout" ] && expectation_count=$((expectation_count + 1))
    [ -n "$expect_error" ] && expectation_count=$((expectation_count + 1))
    [ -n "$expect_warning" ] && expectation_count=$((expectation_count + 1))
    if [ "$expectation_count" -gt 1 ]; then
        echo "gen-manifest: multiple expectations in $src" >&2
        exit 1
    fi
    if [ -n "$expect_run" ]; then
        echo "run|$(basename "$src")|$expect_run||${xcc_args:-}|${link_args:-}"
    elif [ -n "$expect_stdout" ]; then
        echo "stdout|$(basename "$src")|0|$expect_stdout|${xcc_args:-}|${link_args:-}"
    elif [ -n "$expect_error" ]; then
        echo "xcc-error|$(basename "$src")|1|$expect_error|${xcc_args:-}|${link_args:-}"
    elif [ -n "$expect_warning" ]; then
        echo "xcc-warning|$(basename "$src")|0|$expect_warning|${xcc_args:-}|${link_args:-}"
    else
        echo "gen-manifest: missing /* expect: N */, /* expect-stdout: text */, /* expect-error: text */, or /* expect-warning: text */ in $src" >&2
        exit 1
    fi
done

for src in "$ABI_DIR"/*_xcc.c; do
    [ -e "$src" ] || continue
    expect_run=$(sed -n 's@^/[*] expect: \([0-9][0-9]*\) [*]/$@\1@p' "$src" | sed -n '1p')
    abi_role=$(sed -n 's@^/[*] abi-role: \(caller\|callee\) [*]/$@\1@p' "$src" | sed -n '1p')
    abi_peer=$(sed -n 's@^/[*] abi-peer: \(.*\) [*]/$@\1@p' "$src" | sed -n '1p')

    if [ -z "$expect_run" ] || [ -z "$abi_role" ] || [ -z "$abi_peer" ]; then
        echo "gen-manifest: incomplete ABI metadata in $src" >&2
        exit 1
    fi
    if [ ! -f "$ABI_DIR/$abi_peer" ]; then
        echo "gen-manifest: missing ABI peer $ABI_DIR/$abi_peer for $src" >&2
        exit 1
    fi
    echo "abi-$abi_role|$(basename "$src")|$expect_run|$abi_peer|"
done
