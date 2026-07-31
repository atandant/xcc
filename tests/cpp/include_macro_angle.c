/* expect: 9 */
/* cpp-flags: -Itests/cpp/fixtures/include/angle */
#define HEADER <angle.h>
#include HEADER
int main(void) { return ANGLE_VALUE; }
