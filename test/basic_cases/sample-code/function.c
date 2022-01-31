#include <stdio.h>

int gv;
int gc = 2;

int product(int a, int b) {
    int result;
    result = a * b;
    return result;
}

int sum(int a, int b) {
    int result;
    result = a + b;
    return result;
}

int dot(int x1, int y1, int x2, int y2) {
    int result;
    result = sum(product(x1, y1), product(x2, y2));
    return result;
}

int main() {
    int lv;
    int lc = 2;

    gv = 2;
    lv = 2;

    gv = product(gv, gc);
    lv = gv + product(lv, lc);

    printf("%d\n", gv);
    printf("%d\n", lv);

    gv = dot(gv, gc, lv, lc);

    return 0;
}