#include <stdio.h>
#include <sys/time.h>

int main(int argc, char * argv[])
{
    struct timeval tv;
    int i, j;
    for (i = 0; i < 1000; i++)
    { 
         j = gettimeofday(&tv, NULL);
    }
    return j;
}

