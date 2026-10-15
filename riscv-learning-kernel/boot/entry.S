    .section .text.boot, "ax"
    .globl _start
    .type _start, @function
_start:
    # OpenSBI가 M-mode 초기화를 마치고 이 주소로 S-mode 진입시킨다.
    # make run은 -smp 1: 하나의 hart만 이 스택과 BSS를 사용한다.
    csrci sstatus, 2             # SIE(bit 1)=0: 아직 인터럽트 처리기가 없다.
    csrw sie, zero

    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top           # 스택은 아래로 자라며 16바이트 정렬한다.

    # C의 전역/static 미초기화 변수를 0으로 만든다. 스택은 BSS 밖에 있다.
    la t0, __bss_start
    la t1, __bss_end
1:
    bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:
    call kernel_main
3:
    wfi                         # kernel_main이 돌아오는 경우의 안전한 정지 루프
    j 3b
    .size _start, . - _start
