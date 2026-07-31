/* expect: 6 */
/* expect-note: #pragma message: escaped\ntext */
#pragma message "escaped\ntext"
int main(void) { return 6; }
