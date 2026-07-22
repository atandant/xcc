typedef int (*F)(int);

int f(int x) {
    return x + 1;
}

int main(void) {
    F a = f;
    int c = a(a(a(5)));
    void *p = &c;
    int *q = *(int **)&p;
    return *q - 8;
}
