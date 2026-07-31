/* expect: 16 */
/* expect-warning: malformed #pragma message: expected string literal */
#pragma message("text" extra)
int main(void) { return 16; }
