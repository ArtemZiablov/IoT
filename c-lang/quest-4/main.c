#include <stdio.h>
#include <stdlib.h>

#define LENGTH (12)

int main(int argc, char* argv[]) {
    int* values = (int*) malloc(LENGTH * sizeof(int));

    for (int i = 0; i < LENGTH; i++) {
        values[i] = i;
    }

    for (int i = 0; i < LENGTH/2; i++) {
        // Step 1: (long*) values
        // Take the pointer values (which is int*) and lie to the compiler — 
        // tell it "pretend this points to longs instead." The actual bytes in 
        // memory don't change at all, only how we interpret them.
        
        // Step 2: [i]
        // Now index into it as if it's an array of longs. Since long is 8 bytes,
        // each step jumps 8 bytes forward instead of 4.
        
        // Step 3: (int)
        // Cast the result back to int, which keeps only the lower 32 bits 
        // and throws the upper 32 bits away.
        
        printf("%u: %d\n", i, (int) (((long*) values)[i]));
    }

    return 0;
}
