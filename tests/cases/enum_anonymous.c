/* SPDX-License-Identifier: MIT */
/* expect: 6 */
/* Anonymous enum: a tagless enum is just a way to name int constants. */
enum { OK = 6, ERR = 7 };

int main(void) {
    return OK;
}
