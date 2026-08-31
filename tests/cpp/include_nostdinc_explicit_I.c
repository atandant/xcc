/* expect: 8 */
/* cpp-flags: -nostdinc -Iinclude */
#include <stddef.h>
int main(void) { return sizeof(size_t); }
