/*dead code example*/

int add(int x, int y) {
    return x+y;
}

/*int defectiveadd(int x, int y) {
    return x+y;
}*/
int math_test(int x) {
    int a = x * 2;
    int b = 999;  
    
    if (0) {
        return b;   
    }
    
    return a;
}

int main() {
    add(5, 9);
    math_test(5);
    return 0;
}

