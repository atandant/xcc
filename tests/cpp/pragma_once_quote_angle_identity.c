/* expect: 14 */
/* cpp-flags: -Itests/cpp/fixtures/include */
#include "fixtures/include/pragma/identity.h"
#include <pragma/identity.h>
int main(void) { return pragma_once_identity(); }
