#include "drivers/uart.h"
#include "console.h"
#include "shell.h"

void kernel_main(void)
{
    uart_init();
    console_write("Hello Kernel\n");
    console_write("RISC-V S-mode shell. Type help.\n");
    shell_run();
}
