/* SPDX-License-Identifier: MIT */
/* expect: 4 */
static int state;

static void mark(int expected) {
    if (state == expected)
        state = state + 1;
    else
        state = 100;
}

static void exercise(void *p) {
    ((void)p, (void)0);
    (mark(0), (void)mark(1));
    ((void)(mark(2), 99), (void)mark(3));
}

int main(void) {
    int value;
    value = 0;
    exercise(&value);
    return state;
}
