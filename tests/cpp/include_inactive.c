/* expect: 6 */
#if 0
#include "this-header-must-not-be-opened.h"
#endif
int main(void) { return 6; }
