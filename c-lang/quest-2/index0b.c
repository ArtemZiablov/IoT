#include <stdio.h>
#include <stdlib.h>

#define SIZE 10

int main (int argc, char* argv[]){
    int* a = (int*)malloc(SIZE*sizeof(int));
    for(int i = 0; i < SIZE; i++){
        a[i] = i;
        printf("index %d : value = %d\n", i, a[i]);
    }
    free(a);
    return 0;
}