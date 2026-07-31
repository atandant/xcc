/* expect-error: macro 'VALUE' redefined with different replacement */
/* cpp-flags: -DVALUE=1 -DVALUE=2 */
int main(void) { return VALUE; }
