/* expect-error: #error nested include failure */
/* expect-include-count: 2 */
/* expect-include: In file included from tests/cpp/fixtures/include/diagnostic/outer.h:1:10: */
#include "fixtures/include/diagnostic/outer.h"
int main(void) { return 0; }
