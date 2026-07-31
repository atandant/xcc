/* expect-error: malformed #pragma message: expected string literal */
/* cpp-flags: -Werror=pragmas */
#pragma message malformed
int main(void) { return 0; }
