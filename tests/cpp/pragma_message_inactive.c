/* expect: 11 */
/* expect-no-note: #pragma message: inactive message */
#if 0
#pragma message "inactive message"
#endif
int main(void) { return 11; }
