/* expect: 13 */
/* expect-note: #pragma message: nested macro */
#define INNER "nested macro"
#define OUTER INNER
#pragma message(OUTER)
int main(void) { return 13; }
