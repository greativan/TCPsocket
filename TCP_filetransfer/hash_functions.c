#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

#define BLOCK_SIZE 8

char* hash(FILE *f) {
    char *hash_val;
    hash_val = malloc(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; i++){
        hash_val[i] = '\0';
    }
    char string;
    int block_index = 0;
    while(fread(&string, 1, 1, f) == 1){
        hash_val[block_index] = string ^ hash_val[block_index];
        block_index += 1;
        if (block_index == BLOCK_SIZE){
            block_index = 0;
        }
    }
    return hash_val;
}


int check_hash(const char *hash1, const char *hash2) {
    for (long i = 0; i < BLOCK_SIZE; i++) {
        if (hash1[i] != hash2[i]) {
            printf("Index %ld: %c\n", i, hash1[i]);
            return 1;
        }
    }
    return 0;
}
