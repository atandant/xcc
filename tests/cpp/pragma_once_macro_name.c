/* expect: 2 */
#include "fixtures/include/pragma/macro-name.h"
#include "fixtures/include/pragma/macro-name.h"
int main(void) { return PRAGMA_MACRO_NAME_COUNT; }
