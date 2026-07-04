/* SPDX-License-Identifier: MIT */
/* Simple unsigned-byte checksum over a tiny buffer. */
int main(void) {
    unsigned char buf[4];
    unsigned int sum;
    int i;

    buf[0] = 10;
    buf[1] = 20;
    buf[2] = 30;
    buf[3] = 40;
    sum = 0;
    i = 0;
    while (i < 4) {
        sum = sum + buf[i];
        i = i + 1;
    }
    return (int)sum;
}
