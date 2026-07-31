/* expect: 5 */
/* expect-note: #pragma message: adjacent strings */
#pragma message "adjacent " "strings"
int main(void) { return 5; }
