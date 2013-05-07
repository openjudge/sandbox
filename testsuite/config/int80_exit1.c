int main(void)
{
    asm volatile("movl $1, %ebx\n"
                 "movl $1, %eax\n"
                 "int $0x80\n");
    for (;;);
    return 0;
}
