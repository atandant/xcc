/* expect: 17 */
/* cpp-flags: -Wno-pragmas */
/* expect-no-warning: malformed #pragma message: expected string literal */
#pragma message malformed
int main(void) { return 17; }
