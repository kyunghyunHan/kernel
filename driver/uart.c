void uart_putchar(char c)  // 이 이름이 더 정확해
{
    volatile uint8_t *uart = (volatile uint8_t *)0x10000000UL;

    while ((uart[5] & (1U << 5)) == 0) {
    }

    uart[0] = (uint8_t)c;
}