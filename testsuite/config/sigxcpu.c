#include <sys/resource.h> 
#include <sys/time.h> 
#include <unistd.h> 
 
int main () 
{
    struct rlimit rl; 
 
    /* Obtain the current limits. */ 
    getrlimit (RLIMIT_CPU, &rl); 

    /* Set a CPU limit of 1 second. */ 
    rl.rlim_cur = 0;
    rl.rlim_max = 30;
    setrlimit (RLIMIT_CPU, &rl); 

    /* Do busy work. */ 
    while (1)
    {
        ;
    }

    return 0; /* Never reached */
} 
