#include <stdio.h>

int main(int argc, char * argv[])
{
    long a, b;
    while (!feof(stdin))
    {
        if (scanf("%ld %ld", &a, &b) != 2)
            break;
        printf("%ld\n", a + b);
    }

    return 0;
}

