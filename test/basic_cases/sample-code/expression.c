#include <stdio.h>

int gv;
int gc = 2;

int main() {
    int lv;
    int lc = 2;
    
    gv = 2;
    lv = 2;
    
    gv = lc + gv + gc + lv;
    lv = lc * gv * gc * lv;

    printf("%d\n", gv);
    printf("%d\n", lv);

    gv = lc + ((gv + gc) * lv);
    lv = (lc + (gv + (gc + (lv + (lc + (gv + (gc + (lv + lc))))))));

    printf("%d\n", gv);
    printf("%d\n", lv);
    
    return 0;
}