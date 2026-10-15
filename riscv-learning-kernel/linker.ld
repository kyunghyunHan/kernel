OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
    /* QEMU RAM은 0x80000000부터. 앞 2 MiB는 OpenSBI용으로 남긴다. */
    RAM (rwx) : ORIGIN = 0x80200000, LENGTH = 126M
}

SECTIONS {
    . = ORIGIN(RAM);
    .text : ALIGN(16) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } > RAM
    .rodata : ALIGN(16) {
        *(.rodata .rodata.* .srodata .srodata.*)
    } > RAM
    .data : ALIGN(16) {
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.* .sdata .sdata.*)
    } > RAM
    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM
    .stack (NOLOAD) : ALIGN(16) {
        __stack_bottom = .;
        . += 16K;
        __stack_top = .;
    } > RAM
    /DISCARD/ : { *(.eh_frame .eh_frame.* .comment) }
}

ASSERT(_start == ORIGIN(RAM), "entry must be at 0x80200000")
ASSERT((__stack_top & 15) == 0, "stack must be 16-byte aligned")
