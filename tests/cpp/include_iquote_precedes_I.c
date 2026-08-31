/* expect: 32 */
/* cpp-flags: -Itests/cpp/fixtures/include/user -iquotetests/cpp/fixtures/include/iquote */
#include "choice.h"
int main(void) { return INCLUDE_CHOICE_VALUE; }
