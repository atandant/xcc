/* expect: 14 */
/* expect-warning: malformed #pragma message: expected string literal */
#pragma message not_a_string
int main(void) { return 14; }
