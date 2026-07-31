/* XTEA encrypt/decrypt round trip.
 *
 * This is a compact stress test for 32-bit unsigned arithmetic, shifts,
 * XOR-heavy expression trees, indexed key loads, and register allocation in
 * a tight loop.  main returns zero when decrypting restores the input block.
 */

void xtea_encrypt(unsigned int block[2], unsigned int key[4])
{
    unsigned int v0;
    unsigned int v1;
    unsigned int sum;
    unsigned int delta;
    int i;

    v0 = block[0];
    v1 = block[1];
    sum = 0U;
    delta = 0x9e3779b9U;
    for (i = 0; i < 32; i = i + 1) {
        v0 = v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^
                   (sum + key[sum & 3U]));
        sum = sum + delta;
        v1 = v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^
                   (sum + key[(sum >> 11) & 3U]));
    }
    block[0] = v0;
    block[1] = v1;
}

void xtea_decrypt(unsigned int block[2], unsigned int key[4])
{
    unsigned int v0;
    unsigned int v1;
    unsigned int sum;
    unsigned int delta;
    int i;

    v0 = block[0];
    v1 = block[1];
    delta = 0x9e3779b9U;
    sum = 0xc6ef3720U;
    for (i = 0; i < 32; i = i + 1) {
        v1 = v1 - ((((v0 << 4) ^ (v0 >> 5)) + v0) ^
                   (sum + key[(sum >> 11) & 3U]));
        sum = sum - delta;
        v0 = v0 - ((((v1 << 4) ^ (v1 >> 5)) + v1) ^
                   (sum + key[sum & 3U]));
    }
    block[0] = v0;
    block[1] = v1;
}

int main(void)
{
    unsigned int block[2];
    unsigned int key[4];
    unsigned int encrypted0;
    unsigned int encrypted1;

    block[0] = 0x12345678U;
    block[1] = 0x9abcdef0U;
    key[0] = 0x6a1d78c8U;
    key[1] = 0x8c86d67fU;
    key[2] = 0x2a65bfbeU;
    key[3] = 0xb4bd6e46U;

    xtea_encrypt(block, key);
    encrypted0 = block[0];
    encrypted1 = block[1];
    xtea_decrypt(block, key);

    if (encrypted0 == 0x12345678U && encrypted1 == 0x9abcdef0U)
        return 1;
    if (block[0] != 0x12345678U)
        return 2;
    if (block[1] != 0x9abcdef0U)
        return 3;
    return 0;
}
