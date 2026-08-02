/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
/* link-args: -lm */
#include <math.h>

int main(void)
{
    if (sin(0.0) > 1e-9 || sin(0.0) < -1e-9)
        return 1;
    if (cos(0.0) < 0.999999 || cos(0.0) > 1.000001)
        return 2;
    if (sqrt(4.0) < 1.999999 || sqrt(4.0) > 2.000001)
        return 3;
    if (fabs(-3.5) < 3.499999 || fabs(-3.5) > 3.500001)
        return 4;
    if (floor(1.75) != 1.0)
        return 5;
    if (ceil(1.25) != 2.0)
        return 6;
    return 0;
}
