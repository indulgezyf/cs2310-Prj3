#include <stdio.h>
int main() {
    char a[10]={0};
    long offset = 1;
    if(a+offset == &a[offset])
    {
        printf("yes\n");
    }
    else
    {
        printf("no\n");
    }
    return 0;
}