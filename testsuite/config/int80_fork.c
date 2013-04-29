int main(void)
{
    int pid;
    asm volatile("movl $2, %%eax\n"
                 "int $0x80\n"
                 "movl %%eax, %0\n": "=r"(pid));
    for (;;);
    return 0;
}
