/* expect: 12 */
/* expect-note: #pragma message: comment spacing */
#pragma message "comment "/**/"spacing"
int main(void) { return 12; }
