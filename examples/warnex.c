/* SPDX-License-Identifier: MIT */
/*
 * warnex.c — compile-time warning tour for xcc.
 *
 * Exercises default-on warnings (old-style definitions, implicit and
 * unprototyped calls, char overflow, bare return) while still producing
 * a runnable program. Diagnostics go to stderr; stdout is unused.
 *
 *   ./xcc examples/warnex.c -o /tmp/warnex.s 2>/tmp/warnex.err
 *   gcc /tmp/warnex.s -o /tmp/warnex && /tmp/warnex; echo $?
 *   # -> 42
 *
 * Marked xcc-expect-warning for examples/build.sh (warnings are expected).
 */

int bump_n() { return 3; }
int tweak_n() { return 5; }

int leaky() {
    return;
}

int scale();
int mix();

char encode() {
    return 300;
}

/*void stash(char c) {
    (void)c;
}*/

int main() {
    char c;
    char d = 400;
    char *p;
    int n;

    n = fetch();
    n = n + fetch();
    n = n + scale(2);
    n = n + mix(1, 2);
    n = n + bump_n();
    n = n + tweak_n();

    leaky();

    c = 300;
    p = &c;
    *p = 500;
    return 42;
}

int fetch() { return 7; }

int scale(int x) { return x; }

int mix(int a, int b) { return a + b; }
