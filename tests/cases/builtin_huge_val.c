/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* link-args: -lm */
#include <float.h>
#include <math.h>

int main(void)
{
    double positive = HUGE_VAL;
    double negative = -HUGE_VAL;

    return !(positive > DBL_MAX) || !(negative < -DBL_MAX) ||
           positive != HUGE_VAL || negative != -HUGE_VAL;
}
