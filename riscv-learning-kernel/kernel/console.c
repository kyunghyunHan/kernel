#include "console.h"
#include "drivers/uart.h"

void console_putc(char c)
{
    /* 터미널에서 새 줄 시작 위치까지 이동하도록 LF 앞에 CR을 붙인다. */
    if (c == '\n') {
        uart_putc('\r');
    }
    uart_putc(c);
}

void console_write(const char *text)
{
    while (*text != '\0') {
        console_putc(*text++);
    }
}

char console_getc(void)
{
    return uart_getc();
}

void console_clear(void)
{
    /* ANSI 터미널: 화면 지우기, 커서를 첫 행/첫 열로 이동. */
    console_write("\033[2J\033[H");
}
