#include <stdio.h>

int sum(int a[10]) {
    int result;
    result = a[1] + a[2];
    return result;
}

int main(){
    int a[10];
    a[1] = 10;
    a[2] = 5;

    printf("%d\n", a[1]);
    printf("%d\n", a[2]);
    printf("%d\n", sum(a));
    scanf("%d\n", &a[3]);
    printf("%d\n", a[3]);

    if (a[1] > a[2])
        printf("%d\n", a[1]);
    else
        printf("%d\n", a[2]);

    return 0;
}