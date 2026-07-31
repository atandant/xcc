/* expect: 9 */
/* expect-note: #pragma message: trigraph message */
??=pragma message "trigraph message"
int main(void) { return 9; }
