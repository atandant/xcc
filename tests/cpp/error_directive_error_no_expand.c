/* expect-error: #error MESSAGE */
#define MESSAGE expanded
#error MESSAGE
int main(void) { return 0; }
