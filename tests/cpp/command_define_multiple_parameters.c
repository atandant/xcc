/* expect: 11 */
/* cpp-flags: -DADD(x,y)=((x)+(y)) */
int main(void) { return ADD(5, 6); }
