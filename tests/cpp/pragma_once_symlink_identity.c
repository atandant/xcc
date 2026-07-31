/* expect: 14 */
/* pragma-identity-fixture */
#include <identity-original.h>
#include <identity-symlink.h>
int main(void) { return pragma_once_identity(); }
