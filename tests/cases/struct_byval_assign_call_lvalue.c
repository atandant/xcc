/* SPDX-License-Identifier: MIT */
/* expect: 42 */
typedef struct {
    int first;
    int second;
} Pair;

typedef struct {
    long first;
    long second;
    long third;
} Big;

static Pair make_pair(void) {
    Pair value;
    value.first = 10;
    value.second = 11;
    return value;
}

static Big make_big(void) {
    Big value;
    value.first = 6;
    value.second = 7;
    value.third = 8;
    return value;
}

int main(void) {
    Pair pairs[1];
    Big big;
    Big *p;
    p = &big;
    pairs[0] = make_pair();
    *p = make_big();
    return pairs[0].first + pairs[0].second +
           (int)(big.first + big.second + big.third);
}
