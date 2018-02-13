#include <stdio.h>
#include <string.h>
#include "ftree.h"
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#ifndef PORT
  #define PORT 30000
#endif


int main(int argc, char **argv) {
    /* Note: In most cases, you'll want HOST to be localhost or 127.0.0.1, so 
     * you can test on your local machine.*/
    //char *dest;
    if (argc != 3) {
        printf("Usage:\n\trcopy_client SRC HOST\n");
        printf("\t SRC - The file or directory to copy to the server\n");
        printf("\t HOST - The hostname of the server");
        return 1;
    }

    if (rcopy_client(argv[1], argv[2], PORT) != 0) {
        printf("Errors encountered during copy\n");
        return 1;
    } else {
        printf("Copy completed successfully\n");
        return 0;
    }
}

