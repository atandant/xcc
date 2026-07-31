/* expect-error: #endif without matching #if */
#if 1
#include "fixtures/include/conditional/close_parent.h"
#endif
int main(void) { return 0; }
