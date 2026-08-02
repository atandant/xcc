#!/bin/sh
# SPDX-License-Identifier: MIT
# Acceptance harness: regenerate the manifest, then run executable and
# diagnostic cases against it.
set -u

XCC=./xcc
DIR="tests/cases"
ABI_DIR="tests/abi"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MANIFEST="$TMP/manifest.txt"
./tests/gen-manifest.sh "$DIR" "$ABI_DIR" > "$MANIFEST" || exit 1

pass=0
fail=0

while IFS='|' read -r kind file expected expected_err xcc_args link_args; do
    case "$kind" in
        ''|\#*) continue ;;
    esac

    case "$kind" in
        abi-*) src="$ABI_DIR/$file" ;;
        *)     src="$DIR/$file" ;;
    esac
    case "$kind" in
        run)
            # shellcheck disable=SC2086
            if ! $XCC $xcc_args "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
                echo "FAIL $file (xcc error)"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
                continue
            fi

            if ! gcc "$TMP/out.s" -o "$TMP/out" ${link_args:-} 2> "$TMP/gccerr"; then
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
            # shellcheck disable=SC2086
            if $XCC $xcc_args "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
                echo "FAIL $file: expected xcc error, got success"
                fail=$((fail + 1))
                continue
            fi

            if grep ': error: ' "$TMP/err" | sed 's/^.*: error: //' | grep -Fx "$expected_err" > /dev/null; then
                echo "ok   $file -> xcc error"
                pass=$((pass + 1))
            else
                echo "FAIL $file: expected primary error message '$expected_err'"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            fi
            ;;
        xcc-warning)
            # shellcheck disable=SC2086
            if ! $XCC $xcc_args "$src" -o "$TMP/out.s" 2> "$TMP/err"; then
                echo "FAIL $file (xcc error, expected warning only)"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
                continue
            fi

            if grep ': warning: ' "$TMP/err" | sed 's/^.*: warning: //' | grep -Fx "$expected_err" > /dev/null; then
                echo "ok   $file -> xcc warning"
                pass=$((pass + 1))
            else
                echo "FAIL $file: expected primary warning message '$expected_err'"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
            fi
            ;;
        abi-caller|abi-callee)
            peer="$ABI_DIR/$expected_err"
            if ! $XCC "$src" -o "$TMP/abi-xcc.s" 2> "$TMP/err"; then
                echo "FAIL $file (xcc ABI compile error)"
                sed 's/^/      /' "$TMP/err"
                fail=$((fail + 1))
                continue
            fi
            if ! gcc -c "$TMP/abi-xcc.s" -o "$TMP/abi-xcc.o" 2> "$TMP/gccerr"; then
                echo "FAIL $file (assemble error)"
                sed 's/^/      /' "$TMP/gccerr"
                fail=$((fail + 1))
                continue
            fi
            if ! gcc -std=c89 -pedantic-errors -c "$peer" -o "$TMP/abi-gcc.o" \
                    2> "$TMP/gccerr"; then
                echo "FAIL $file (gcc ABI peer compile error)"
                sed 's/^/      /' "$TMP/gccerr"
                fail=$((fail + 1))
                continue
            fi
            if ! gcc "$TMP/abi-xcc.o" "$TMP/abi-gcc.o" -o "$TMP/abi-out" \
                    2> "$TMP/gccerr"; then
                echo "FAIL $file (ABI link error)"
                sed 's/^/      /' "$TMP/gccerr"
                fail=$((fail + 1))
                continue
            fi

            "$TMP/abi-out"
            got=$?
            if [ "$got" = "$expected" ]; then
                echo "ok   $file + $expected_err -> $got"
                pass=$((pass + 1))
            else
                echo "FAIL $file + $expected_err: expected $expected, got $got"
                fail=$((fail + 1))
            fi
            ;;
        *)
            echo "FAIL manifest: unknown test kind '$kind' for $file"
            fail=$((fail + 1))
            ;;
    esac
done < "$MANIFEST"

# Optimizer checks must inspect LIR: runtime equivalence alone does not prove
# that the intended rewrites happened.
if ./tests/check-lir-opt.sh "$XCC"; then
    echo "ok   lir-optimizer-transforms"
    pass=$((pass + 1))
else
    echo "FAIL lir-optimizer-transforms: expected optimizer rewrites"
    fail=$((fail + 1))
fi

# CLI-only checks (no dedicated source file).
if ! $XCC -Wnot-a-real-warning "$DIR/warn_implicit_function_decl.c" -o "$TMP/out.s" 2> "$TMP/err"; then
    if grep -q "unknown warning option '-Wnot-a-real-warning'" "$TMP/err"; then
        echo "ok   cli-unknown-wflag"
        pass=$((pass + 1))
    else
        echo "FAIL cli-unknown-wflag: expected unknown warning option diagnostic"
        sed 's/^/      /' "$TMP/err"
        fail=$((fail + 1))
    fi
else
    echo "FAIL cli-unknown-wflag: expected failure"
    fail=$((fail + 1))
fi

# Every generated object should opt out of an executable process stack.
if $XCC "$DIR/ret_const.c" -o "$TMP/out.s" 2> "$TMP/err" &&
   grep -q '^  \.section \.note\.GNU-stack,"",@progbits$' "$TMP/out.s"; then
    echo "ok   assembly-non-executable-stack"
    pass=$((pass + 1))
else
    echo "FAIL assembly-non-executable-stack: expected GNU stack note"
    sed 's/^/      /' "$TMP/err"
    fail=$((fail + 1))
fi

# Allocation dumps provide stable metrics for tracking allocator changes.
if $XCC --xcc-dump-lir-alloc "$DIR/regalloc_split_many_params.c" \
        > "$TMP/alloc" 2> "$TMP/err" &&
   grep -q '^allocation pressure$' "$TMP/alloc" &&
   grep -Eq '^  v[0-9]+ fragment-of=v[0-9]+ = (%[a-z0-9]+|stack\(-[0-9]+\))$' "$TMP/alloc" &&
   grep -Eq '^  v[0-9]+ \[[0-9]+, [0-9]+\] weight=[1-9][0-9]* ranges\([1-9][0-9]*\): \[[0-9]+,[0-9]+\].* positions\([1-9][0-9]*\): .*[0-9]+u@[1-9][0-9]*' "$TMP/alloc" &&
   grep -Eq '^  metrics live=[0-9]+ registers=[0-9]+ spilled=[0-9]+ spill-slots=[0-9]+ callee-saved=[0-9]+ range-reuses=[1-9][0-9]* splits=[1-9][0-9]* fragments=[1-9][0-9]* split-moves=[1-9][0-9]* outgoing=[0-9]+ frame=[0-9]+$' "$TMP/alloc"; then
    echo "ok   cli-regalloc-metrics"
    pass=$((pass + 1))
else
    echo "FAIL cli-regalloc-metrics: expected allocation metrics"
    sed 's/^/      /' "$TMP/err"
    sed 's/^/      /' "$TMP/alloc"
    fail=$((fail + 1))
fi

# Diagnostics must retain a physical source line longer than the old 4096-byte
# read buffer, without splitting it or reading past the captured text.
awk 'BEGIN { for (i = 0; i < 5000; i++) printf " "; print "@" }' > "$TMP/long-line.c"
if $XCC "$TMP/long-line.c" -o "$TMP/out.s" 2> "$TMP/err"; then
    echo "FAIL cli-long-diagnostic-line: expected failure"
    fail=$((fail + 1))
elif grep -q ':1:5001: error: ' "$TMP/err"; then
    echo "ok   cli-long-diagnostic-line"
    pass=$((pass + 1))
else
    echo "FAIL cli-long-diagnostic-line: expected diagnostic at line 1, column 5001"
    sed 's/^/      /' "$TMP/err"
    fail=$((fail + 1))
fi

echo "----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
