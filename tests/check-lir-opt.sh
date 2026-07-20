#!/bin/sh
# SPDX-License-Identifier: MIT
# Check optimizer transformations that runtime acceptance tests cannot observe.
set -u

XCC=${1:-./xcc}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/opt-lir.c" <<'EOF'
long mul_left(long x) { return 8 * x; }
long ptr_diff(long *a, long *b) { return a - b; }
int sub_self(int x) { return x - x; }
int add_zero(int x) { return x + 0; }
int and_zero(int x) { return x & 0; }
int or_zero(int x) { return x | 0; }
int xor_zero(int x) { return x ^ 0; }
int shift_zero(int x) { return x << 0; }
int mem2reg_loop(int n) {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < n) { sum = sum + i; i = i + 1; }
    return sum;
}
int mem2reg_address_taken(void) {
    int x;
    int *p;
    x = 1;
    p = &x;
    *p = 2;
    return x;
}
int mem2reg_narrow(void) {
    char c;
    c = 300;
    return c;
}
int licm_conditional(int n, int flag, int x, int y) {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < n) {
        if (flag)
            sum = sum + x * y;
        i = i + 1;
    }
    return sum;
}
int licm_variant(int n, int x) {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < n) {
        sum = sum + x * i;
        i = i + 1;
    }
    return sum;
}
int licm_no_div_speculation(int n, int flag, int x, int y) {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < n) {
        if (flag)
            sum = sum + x / y;
        i = i + 1;
    }
    return sum;
}
EOF

if ! "$XCC" --xcc-dump-lir "$TMP/opt-lir.c" > "$TMP/opt-lir" 2> "$TMP/err"; then
    sed 's/^/      /' "$TMP/err"
    exit 1
fi

for fn in licm_conditional licm_variant licm_no_div_speculation; do
    sed -n "/function $fn /,/^}/p" "$TMP/opt-lir" > "$TMP/$fn"
done

conditional_mul=$(grep -nE '^[[:space:]]+[0-9]+[[:space:]]+mul[[:space:]]' "$TMP/licm_conditional" | cut -d: -f1)
conditional_header=$(grep -n 'branch.lt' "$TMP/licm_conditional" | head -n 1 | cut -d: -f1)
variant_mul=$(grep -nE '^[[:space:]]+[0-9]+[[:space:]]+mul[[:space:]]' "$TMP/licm_variant" | cut -d: -f1)
variant_header=$(grep -n 'branch.lt' "$TMP/licm_variant" | head -n 1 | cut -d: -f1)
division=$(grep -nE '^[[:space:]]+[0-9]+[[:space:]]+div[[:space:]]' "$TMP/licm_no_div_speculation" | cut -d: -f1)
division_header=$(grep -n 'branch.lt' "$TMP/licm_no_div_speculation" | head -n 1 | cut -d: -f1)

if sed -n '/function mul_left /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+shl[[:space:]]' &&
   ! sed -n '/function mul_left /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+mul[[:space:]]' &&
   sed -n '/function ptr_diff /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+sdiv_pow2[[:space:]]' &&
   ! sed -n '/function ptr_diff /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+div[[:space:]]' &&
   sed -n '/function sub_self /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+movi.*= 0' &&
   ! sed -n '/function sub_self /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+sub[[:space:]]' &&
   ! sed -n '/function add_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+add[[:space:]]' &&
   ! sed -n '/function and_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+and[[:space:]]' &&
   ! sed -n '/function or_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+or[[:space:]]' &&
   ! sed -n '/function xor_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+xor[[:space:]]' &&
   ! sed -n '/function shift_zero /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+shl[[:space:]]' &&
   ! sed -n '/function mem2reg_loop /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+store[[:space:]]' &&
   [ "$(sed -n '/function mem2reg_loop /,/^}/p' "$TMP/opt-lir" | grep -Ec '^[[:space:]]+[0-9]+[[:space:]]+load[[:space:]]')" -eq 3 ] &&
   sed -n '/function mem2reg_address_taken /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+store[[:space:]]' &&
   sed -n '/function mem2reg_narrow /,/^}/p' "$TMP/opt-lir" | grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+store[[:space:]]' &&
   [ -n "$conditional_mul" ] && [ "$conditional_mul" -lt "$conditional_header" ] &&
   [ -n "$variant_mul" ] && [ "$variant_mul" -gt "$variant_header" ] &&
   [ -n "$division" ] && [ "$division" -gt "$division_header" ]; then
    exit 0
fi

sed 's/^/      /' "$TMP/err"
sed 's/^/      /' "$TMP/opt-lir"
exit 1
