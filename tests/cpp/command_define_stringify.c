/* expect: 97 */
/* cpp-flags: -DSTRINGIFY(x)=#x */
int main(void) { return STRINGIFY(abc)[0]; }
