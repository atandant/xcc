/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate 'volatile' type qualifier */
typedef volatile int VolatileInt;
volatile VolatileInt value;
int main(void) { return value; }
