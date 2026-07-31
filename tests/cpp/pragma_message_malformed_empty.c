/* expect: 15 */
/* expect-warning: malformed #pragma message: expected string literal */
#pragma message
int main(void) { return 15; }
