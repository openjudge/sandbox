#include <stdio.h>

int main(int argc, char * argv[])
{
    while (1)
    {
        fprintf(stdout, "Hello World!\n");
        fflush(stdout);
    }
    return 0;
}

