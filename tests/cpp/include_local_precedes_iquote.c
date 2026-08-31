/* expect: 33 */
/* cpp-flags: -iquote tests/cpp/fixtures/include/iquote */
#include "fixtures/include/iquote/local/top.h"
int main(void) { return LOCAL_CHOICE_VALUE; }
