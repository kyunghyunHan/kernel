.DEFAULT_GOAL := all

# Apple Silicon Homebrew 경로를 우선 사용. 다른 환경은 make KCC=... KLD=... .
KCC ?= $(if $(wildcard /opt/homebrew/opt/llvm/bin/clang),/opt/homebrew/opt/llvm/bin/clang,clang)
KLD ?= $(if $(wildcard /opt/homebrew/opt/lld/bin/ld.lld),/opt/homebrew/opt/lld/bin/ld.lld,ld.lld)
QEMU ?= qemu-system-riscv64
PYTHON ?= python3

BUILD := build
ELF := $(BUILD)/kernel.elf
OBJECTS := $(BUILD)/boot/entry.o $(BUILD)/drivers/uart.o \
           $(BUILD)/kernel/main.o $(BUILD)/kernel/console.o $(BUILD)/kernel/shell.o
ARCH_FLAGS := --target=riscv64-unknown-elf -march=rv64imac_zicsr -mabi=lp64 \
              -mcmodel=medany -mno-relax -msmall-data-limit=0
CFLAGS := $(ARCH_FLAGS) -std=c11 -O2 -g -ffreestanding -fno-builtin \
          -fno-stack-protector -fno-pic -fno-pie -fno-unwind-tables \
          -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -MMD -MP -I.
QEMU_FLAGS := -machine virt -accel tcg -m 128M -smp 1 -bios default \
              -kernel $(ELF) -nographic -nic none

.PHONY: all run test clean
all: $(ELF)

$(ELF): $(OBJECTS) linker.ld
	$(KLD) -m elf64lriscv --no-relax -T linker.ld -Map=$(BUILD)/kernel.map -o $@ $(OBJECTS)

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(KCC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(KCC) $(ARCH_FLAGS) -g -MMD -MP -c $< -o $@

run: $(ELF)
	$(QEMU) $(QEMU_FLAGS)

test: $(ELF)
	$(PYTHON) tests/smoke.py $(QEMU) $(ELF)

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
