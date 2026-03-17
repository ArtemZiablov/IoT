#include <stdio.h>
#include <stdlib.h>

#define SIZE 10

int main (int argc, char* argv[]){
    int* a = (int*)malloc(SIZE*sizeof(int));

    int* b = a;
    b -= 1;

    for (int i = 1; i <= SIZE; i++) {
        b[i] = i;
    }

    for (int i = 1; i <= SIZE; i++) {
        printf("index %d: %d\n", i, b[i]);
    }
    printf("b[0] = %d\n", b[0]);

    free(a);
    return 0;
}