#include <stdio.h>

int gv;
int gc = 2;

int main() {
    int lv;
    gv = 1;
    lv = 3;

    while (gv <= 3) {
        printf("%d\n", gv);
        gv = gv + 1;
    }
    
    for (int i = 10; i < 13; ++i) {
        printf("%d\n", gv);
    }

    return 0;
}