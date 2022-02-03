#include <stdio.h>

int sum(int a[3][3]) {
    int result;
    result = a[1][2] + a[2][1];
    return result;
}

int main(){
    int a[3][3];
    a[1][2] = 10;
    a[2][1] = 5;

    printf("%d\n", a[1][2]);
    printf("%d\n", a[2][1]);
    printf("%d\n", sum(a));
    scanf("%d\n", &a[0][0]);
    printf("%d\n", a[0][0]);

    if (a[1][2] > a[2][1])
        printf("%d\n", a[1][2]);
    else
        printf("%d\n", a[2][1]);

    return 0;
}