#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

int main(int argc, char * argv[])
{
    unsigned long long BLOCK_SIZE = 1048576; /* 1MB */
    char * ptr = NULL;
    unsigned long i = 0;
    while (1)
    {
        if ((ptr = (char*)malloc(BLOCK_SIZE)) != NULL)
        {
            fprintf(stderr, "%llu kB\n", (++i) * BLOCK_SIZE / 1024);
            memset(ptr, 0, BLOCK_SIZE);
        }
        else
        {
            fprintf(stderr, "failed to allocate memory\n");
        }
        fflush(stderr);
    }
    return 0;
}
