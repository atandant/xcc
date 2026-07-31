/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define STRING(x) #x
int main(void) {
    return STRING("x")[0] == '"' && STRING("x")[1] == 'x' &&
           STRING("x")[2] == '"';
}
