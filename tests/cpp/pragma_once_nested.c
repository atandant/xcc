/* expect: 12 */
#include "fixtures/include/pragma/nested/a.h"
#include "fixtures/include/pragma/nested/b.h"
int main(void) { return pragma_once_shared(); }
