#include <unistd.h>

int main(int argc, char * argv[])
{
    pause(); /* Wakes up upon any signal */
    return 0;
}

