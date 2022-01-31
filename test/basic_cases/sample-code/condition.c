#include <stdio.h>

int gv;
int gc = 2;

int sum(int a, int b) {
    int c;
    c = a + b;
    return c;
}

int main() {
    int lv;
    gv = 1;
    lv = 3;
    
    if (gv == 1)
        printf("%d\n", gv);
    else
        printf("%d\n", lv);

    if (sum(gv, gc) > 4)
        printf("%d\n", gv);
    else
        printf("%d\n", lv);

    return 0;
}