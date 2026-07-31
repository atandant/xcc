/* expect: 3 */
/* expect-note: #pragma message: object macro */
#define MESSAGE_TEXT "object macro"
#pragma message MESSAGE_TEXT
int main(void) { return 3; }
