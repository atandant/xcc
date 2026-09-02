/* SPDX-License-Identifier: MIT */
/* expect: 4 */
static int state;

static void mark(int expected) {
    if (state == expected)
        state = state + 1;
    else
        state = 100;
}

static void choose(int condition, int expected) {
    condition ? mark(expected) : (void)0;
}

int main(void) {
    choose(1, 0);
    choose(0, 99);
    (1 ? (void)mark(1) : (void)mark(99), (void)mark(2));
    (0 ? (void)mark(99) : (void)mark(3));
    return state;
}
