#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PAGESIZE 4096

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s size(megabytes)\n", argv[0]);
        return 1;
    }
    unsigned long long size;
    if (sscanf(argv[1], "%lu", &size) != 1) {
        printf("Expected integer size in megabytes\n");
        return 1;
    }
    int * top;
    size = size * 1024;

    // Hog as much as we can.
    while (size != 0) {
        top = malloc(size);
        if (top == NULL) size = (3 * size) / 4;
        else break;
    }

    printf("Hogging %llu bytes (%llu mb)\n", size, size/(1024*1024));
    unsigned long long cycles = 0;

    // Touch an element of each page once per iteration, to keep page resident in memory.
    for (int i = 0; ; i = (i + PAGESIZE/sizeof(int)) % (size / sizeof(int)) );
//        top[i]++;
/*	if (i == 0) {
		cycles++;
//		if (cycles % 100 == 0) printf("Cycled %d times\n", cycles*100);
	}*/
 //   }
    free(top);
    return 0;
}

