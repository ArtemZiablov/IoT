#include <stdio.h>
#include <stdlib.h>

#define SIZE 10

int main (int argc, char* argv[]){
    int arr[SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printf("Hello, World\n");
    for (int i = 0; i < SIZE; i++) {
        printf("index = %d : value = %d\n", i, arr[i]);
    }
    printf("\n");
    return 0;
}