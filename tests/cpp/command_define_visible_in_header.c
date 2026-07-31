/* expect: 10 */
/* cpp-flags: -DPARENT_VALUE=8 */
#include "fixtures/include/state/state.h"
int main(void) { return HEADER_VALUE; }
