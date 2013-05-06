#include <unistd.h>

int main(int argc, char * argv[])
{
    sleep(10000000); /* Wakes up upon SIGALRM */
    return 0;
}

