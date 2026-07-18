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

while IFS='|' read -r kind file expected expected_err xcc_args; do
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
cat > "$TMP/opt-lir.c" <<'EOF'
long mul_left(long x) { return 8 * x; }
long ptr_diff(long *a, long *b) { return a - b; }
int sub_self(int x) { return x - x; }
int add_zero(int x) { return x + 0; }
int and_zero(int x) { return x & 0; }
int or_zero(int x) { return x | 0; }
int xor_zero(int x) { return x ^ 0; }
int shift_zero(int x) { return x << 0; }
EOF
if $XCC --xcc-dump-lir "$TMP/opt-lir.c" > "$TMP/opt-lir" 2> "$TMP/err" &&
   sed -n '/function mul_left /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+shl[[:space:]]' &&
   ! sed -n '/function mul_left /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+mul[[:space:]]' &&
   sed -n '/function ptr_diff /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+sdiv_pow2[[:space:]]' &&
   ! sed -n '/function ptr_diff /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+div[[:space:]]' &&
   sed -n '/function sub_self /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+movi.*= 0' &&
   ! sed -n '/function sub_self /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+sub[[:space:]]' &&
   ! sed -n '/function add_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+add[[:space:]]' &&
   ! sed -n '/function and_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+and[[:space:]]' &&
   ! sed -n '/function or_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+or[[:space:]]' &&
   ! sed -n '/function xor_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+xor[[:space:]]' &&
   ! sed -n '/function shift_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+shl[[:space:]]'; then
    echo "ok   lir-algebraic-simplification"
    pass=$((pass + 1))
else
    echo "FAIL lir-algebraic-simplification: expected optimizer rewrites"
    sed 's/^/      /' "$TMP/err"
    sed 's/^/      /' "$TMP/opt-lir"
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
