#include <stdio.h>

void printGreen(const char* s){
    printf("\033[1;32m%s$\033[0m ",s);
    fflush(stdout);
}