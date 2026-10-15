#include "uart.h"
#include <stdint.h>

/* QEMU RISC-V virt의 NS16550A UART0 MMIO 물리 주소.
 * STM32와 같이 주소에 레지스터가 매핑되지만, 이 장치는 1바이트 간격이다.
 * volatile: 실제 장치 읽기/쓰기를 컴파일러가 제거하거나 캐시하지 않게 한다.
 * 현재 satp=0(주소 변환 없음)이므로 이 주소를 그대로 접근한다. */
#define UART_BASE ((uintptr_t)0x10000000UL)
#define UART_REG(offset) (*(volatile uint8_t *)(UART_BASE + (offset)))

#define UART_RBR 0u      /* DLAB=0, 읽기: 수신 데이터 */
#define UART_THR 0u      /* DLAB=0, 쓰기: 송신 데이터 */
#define UART_DLL 0u      /* DLAB=1: baud divisor 하위 8비트 */
#define UART_IER 1u      /* DLAB=0: interrupt enable */
#define UART_DLM 1u      /* DLAB=1: baud divisor 상위 8비트 */
#define UART_FCR 2u      /* FIFO control (쓰기) */
#define UART_LCR 3u      /* line control */
#define UART_LSR 5u      /* line status */

#define LCR_8N1       0x03u       /* bits 1:0=11: 8 data, stop=1, parity 없음 */
#define LCR_DLAB      (1u << 7)   /* divisor latch 접근을 선택하는 bit 7 */
#define FCR_ENABLE   (1u << 0)   /* RX/TX FIFO 사용 */
#define FCR_CLEAR_RX (1u << 1)   /* 수신 FIFO 초기화 */
#define FCR_CLEAR_TX (1u << 2)   /* 송신 FIFO 초기화 */
#define LSR_DATA_READY (1u << 0) /* 수신 FIFO에 읽을 바이트가 있다 */
#define LSR_THR_EMPTY  (1u << 5) /* 송신 holding register에 쓸 공간이 있다 */

void uart_init(void)
{
    UART_REG(UART_LCR) = LCR_8N1; /* 먼저 DLAB=0: offset 1이 IER이 되게 한다. */
    UART_REG(UART_IER) = 0;      /* 인터럽트 없이 LSR을 반복 확인(polling)한다. */
    UART_REG(UART_LCR) = LCR_DLAB | LCR_8N1;
    /* virt UART clock 3,686,400 Hz / (16 * divisor 2) = 115,200 baud. */
    UART_REG(UART_DLL) = 2;
    UART_REG(UART_DLM) = 0;
    UART_REG(UART_LCR) = LCR_8N1; /* 다시 offset 0을 RBR/THR로 사용 */
    UART_REG(UART_FCR) = FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX;
}

void uart_putc(char c)
{
    while ((UART_REG(UART_LSR) & LSR_THR_EMPTY) == 0) {
        /* busy wait: 전송할 수 있을 때까지 기다린다. */
    }
    UART_REG(UART_THR) = (uint8_t)c;
}

char uart_getc(void)
{
    while ((UART_REG(UART_LSR) & LSR_DATA_READY) == 0) {
        /* 입력이 없으면 커널은 여기에서 기다린다. */
    }
    return (char)UART_REG(UART_RBR);
}
