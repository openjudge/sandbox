int main(void)
{
    return *(volatile int*)0; /* address 0 must be unexecutable */
}
