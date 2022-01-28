#include <stdio.h>

int gv;
int gc = 2;

int main() {
    int lv;
    int lc = 5;
    gv = 1;
    lv = 3;

    printf("%d\n", gv);
    printf("%d\n", gc);
    printf("%d\n", lv);
    printf("%d\n", lc);

    return 0;
}