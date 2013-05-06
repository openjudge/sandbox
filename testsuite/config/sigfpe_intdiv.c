int main(void)
{
    /* "volatile" needed to eliminate compile-time optimizations */
    volatile int x = 42; 
    volatile int y = 0;
    x=x/y;
    return 0; /* Never reached */
}

