#include <stdio.h>
#include <sys/resource.h> 
#include <sys/time.h> 
#include <unistd.h> 

int main (int argc, char * argv[]) 
{
    struct rlimit rl; 
 
    /* Obtain the current limits. */ 
    getrlimit (RLIMIT_FSIZE, &rl); 

    /* Set a file size limit of 5 bytes. */ 
    rl.rlim_cur = 5; 
    setrlimit (RLIMIT_FSIZE, &rl); 

    /* Do busy work. */ 
    while (1)
    {
        fprintf(stdout, "Hello World!\n"); 
        fflush(stdout);
    }

    return 0; /* Never reached */
} 
