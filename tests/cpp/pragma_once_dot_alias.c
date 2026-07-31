/* expect: 14 */
#include "fixtures/include/pragma/identity.h"
#include "fixtures/include/pragma/./identity.h"
int main(void) { return pragma_once_identity(); }
