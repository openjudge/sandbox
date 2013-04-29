#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "synopsis: exec.exe cmd [arg [...]]\n");
        return EX_USAGE;
    }
    execve(argv[1], &argv[1], NULL);
    /* execve() does not return on success */
    return 1;
}

