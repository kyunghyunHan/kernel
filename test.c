// syscall.c — 커널의 시스템콜 처리
void syscall_handler(/* 저장한 레지스터 정보 */)
{
    // 개념 예시
    switch (요청번호) {
    case 1:
        uart_putchar(전달받은문자);
        break;
    }
}