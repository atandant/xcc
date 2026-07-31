/* expect: 10 */
/* expect-note: #pragma message: spliced message */
#pragma mes\
sage "spliced message"
int main(void) { return 10; }
