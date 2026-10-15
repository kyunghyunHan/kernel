# RISC-V 학습용 커널: 첫 단계

Mac Apple Silicon에서 QEMU의 **RV64 `virt` 머신**을 에뮬레이션한다.
OpenSBI가 M-mode에서 부팅하고, 직접 만든 커널과 셸은 S-mode에서 실행된다.
STM32의 레지스터 제어와 비슷하게 UART MMIO 레지스터를 읽고 써서 입출력한다.

구현 범위는 부팅, 스택/BSS 준비, UART polling, 콘솔, 커널 내부 셸이다.
파일시스템·프로세스·네트워크·유저 모드·시스템콜은 구현하지 않았다.
기존 작업 공간의 파일을 보존하기 위해 이 디렉터리에 별도로 작성했다.

## Mac 도구 설치와 실행

Homebrew가 설치되어 있다는 전제에서:

```sh
xcode-select --install  # Command Line Tools가 없을 때 한 번
brew install llvm lld qemu
brew install python    # make test를 사용할 때만 필요

cd /Users/hangyeonghyeon/Desktop/project/kernel/riscv-learning-kernel
make
make run
```

Clang의 RISC-V 타깃과 LLD의 ELF 링커를 사용한다. 별도의 RISC-V GCC나
OpenSBI 소스 빌드는 필요 없다. `-bios default`가 QEMU에 포함된 OpenSBI를 선택한다.
LLD는 LLVM과 별도 Homebrew formula이므로 `lld`도 설치해야 한다.
Makefile은 `/opt/homebrew/opt/llvm/bin/clang`과
`/opt/homebrew/opt/lld/bin/ld.lld`가 있으면 우선 사용한다.
환경이 다르면 직접 지정할 수 있다:

```sh
make KCC="$(brew --prefix llvm)/bin/clang" KLD="$(brew --prefix lld)/bin/ld.lld"
make run QEMU="$(brew --prefix qemu)/bin/qemu-system-riscv64"
```

`make run`의 실제 QEMU 옵션은 다음과 같다:

```sh
qemu-system-riscv64 -machine virt -accel tcg -m 128M -smp 1 \
  -bios default -kernel build/kernel.elf -nographic -nic none
```

M1의 ARM 가상화 대신 TCG로 RISC-V를 에뮬레이션한다. 이 커널은 단일 hart용이다.
`-smp 1`을 유지해야 한다. `-bios none`은 이 S-mode 진입 규약과 맞지 않는다.
OpenSBI 로그 뒤에 다음처럼 사용할 수 있다:

```text
Hello Kernel
RISC-V S-mode shell. Type help.
kernel> help
help         - show commands
echo [text]  - print text
clear        - clear screen (ANSI)
kernel> echo hello
hello
kernel> clear
```

종료는 **Ctrl-a를 누른 뒤 놓고 x**를 누른다(QEMU의 종료 키).
`clear`는 ANSI escape sequence로 화면을 지우고 커서를 왼쪽 위로 옮긴다.

ASCII 최대 127문자를 한 줄로 받는다. CR, LF, CRLF, Backspace, DEL을 처리하며
탭은 공백 한 칸으로 바꾼다. 길이 초과 시 잘린 명령을 실행하지 않고 행 전체를 버린다.
명령 앞/인자 앞 공백은 건너뛰고 `echo` 본문의 내부 공백은 보존한다.
`echo`만 입력하면 빈 줄을 출력한다. `help`와 `clear` 뒤 인자는 무시한다.
방향키 시퀀스는 무시하며 커서 편집·히스토리·따옴표 해석·한글 입력은 지원하지 않는다.
polling 방식이므로 입력 대기 중에도 CPU를 사용하며, 대량 붙여넣기는 UART FIFO에서
유실될 수 있다.

## 파일 구조

```text
boot/entry.S        S-mode 진입점 _start, gp/sp 준비, BSS 초기화
linker.ld           ELF 시작 주소와 섹션/스택 배치
drivers/uart.c/.h   NS16550A MMIO 초기화와 바이트 입출력
kernel/main.c      커널 초기화 순서
kernel/console.c/.h 문자열 출력, 줄바꿈, 화면 지우기
kernel/shell.c/.h   한 줄 입력, 명령 해석과 실행
Makefile           크로스 컴파일, 링크, QEMU 실행
tests/smoke.py     호스트에서 QEMU UART를 통한 통합 검증
build/             생성된 ELF, 오브젝트, 링크 맵, 검증 로그 (git 제외)
```

## 1. 부팅부터 셸까지

```text
QEMU reset
  → QEMU에 포함된 OpenSBI (M-mode)
  → _start (boot/entry.S, 0x80200000, S-mode)
      → S-mode 인터럽트 비활성화
      → gp, sp 설정 (__stack_top, 16바이트 정렬, 스택 16 KiB)
      → __bss_start .. __bss_end를 0으로 초기화
      → kernel_main()
          → uart_init()
          → console_write("Hello Kernel\n")
          → console_write("RISC-V S-mode shell. Type help.\n")
          → shell_run()
```

QEMU는 ELF의 LOAD 세그먼트를 RAM에 적재한다. RAM은 `0x80000000`부터이고,
앞 2 MiB를 OpenSBI용으로 남겨 커널을 `0x80200000`에 링크했다.
링커의 `ENTRY(_start)`와 `.text.boot` 배치로 진입점과 첫 코드를 일치시킨다.
초기값 있는 데이터는 ELF로 적재하고, BSS는 어셈블리에서 직접 0으로 만든다.
스택은 BSS와 분리했으며 위에서 아래로 자란다. 동적 메모리 할당은 없다.

OpenSBI는 hart ID와 DTB 주소를 `a0`, `a1`로 전달하지만 현재 커널은 사용하지 않는다.
`virt`의 고정 UART 주소를 사용하고, OpenSBI가 준비한 주소 변환 없는 상태(`satp=0`)에서
동작한다. 트랩 처리기는 아직 없으며 S-mode 인터럽트와 UART 인터럽트를 끈다.
`kernel_main()`이 반환하면 `_start`의 `wfi` 루프에 머무르지만,
정상 실행에서는 `shell_run()`이 계속 반복한다.

## 2. `echo hello`의 실제 코드 경로

입력 방향:

```text
터미널 키 입력 → QEMU UART 수신 FIFO
  → uart_getc(): LSR bit 0 확인 → RBR에서 한 바이트 읽기
  → console_getc()
  → shell_read_line(): "echo hello"를 배열에 저장, 화면에 입력 문자 표시
  → Enter: 끝에 '\0' 추가
```

위 화살표는 데이터가 올라오는 방향이다. 함수 호출은 `shell_run()`에서
`shell_read_line()` → `console_getc()` → `uart_getc()` 순서로 내려간다.

명령 해석과 출력 방향:

```text
shell_run()
  → shell_execute(line)
      → 첫 공백을 '\0'으로 바꿔 command="echo", arguments="hello"로 나누기
      → string_equal(command, "echo")
      → console_write(arguments)
          → console_putc('h'), console_putc('e'), ...
              → uart_putc(c)
                  → LSR bit 5 확인 → THR에 한 바이트 쓰기
                      → QEMU serial → 터미널에 hello 표시
      → console_putc('\n') → uart_putc('\r'), uart_putc('\n')
```

입력 중 보이는 `echo hello`는 `shell_read_line()`이 직접 입력 문자를 되돌려 출력한 것이다.
Enter 후 보이는 `hello`는 `shell_execute()`의 `echo` 분기가 출력한 결과다.

## 3. 커널·드라이버·셸의 관계

| 구성 | 실제 함수 | 역할 |
| --- | --- | --- |
| 커널 진입/초기화 | `_start`, `kernel_main()` | C 실행 환경과 하드웨어를 준비하고 셸 시작 |
| UART 드라이버 | `uart_init()`, `uart_putc()`, `uart_getc()` | 장치 주소·레지스터·비트 의미를 알아 실제 바이트 전송 |
| 콘솔 | `console_write()`, `console_putc()`, `console_getc()`, `console_clear()` | 문자열과 터미널 표시를 UART 바이트 입출력으로 바꾸기 |
| 커널 내부 셸 | `shell_run()`, `shell_read_line()`, `shell_execute()` | 사용자 입력을 모으고 명령을 해석해 콘솔 함수를 호출 |

이들은 모두 하나의 `kernel.elf`에 함께 링크되고 동일한 S-mode에서 실행된다.
드라이버는 커널 코드의 일부이며 등록 시스템이나 동적 로딩 없이 직접 호출한다.
셸도 커널 코드의 일부라 잘못된 포인터 접근이 커널 전체를 망가뜨릴 수 있다.

`UART_BASE + 5`는 LSR이다. 예를 들어 `LSR & (1u << 5)`는 다른 비트를 무시하고
송신 가능 여부만 확인한다. `UART_BASE + 0`은 DLAB=0일 때 읽으면 RBR,
쓰면 THR이다. DLAB=1이면 같은 주소가 baud divisor 레지스터가 된다.
이런 주소/비트 선택은 STM32 주변장치 제어와 같은 사고방식이지만 레지스터 배치는 다르다.
구체적인 주소, 비트마스크, divisor 설정은 `drivers/uart.c`에 주석으로 적었다.

`-ffreestanding -fno-builtin`으로 컴파일하고 LLD로 오브젝트만 직접 링크한다.
호스트 `printf`, `write`, libc, 시작 코드, OS SDK를 링크하지 않는다.
`<stdint.h>`, `<stddef.h>`는 컴파일러의 정수형/크기형 정의만 사용한다.
Python과 호스트 프로세스 API는 검증 도구에만 있으며 커널 ELF에는 들어가지 않는다.

## 4. 함수 연결과 시스템콜 번호 연결은 다르다

지금의 `console_write("hello")`는 **일반 C 함수 호출**이다.
헤더가 함수 선언을 제공하고, `.c` 파일이 정의를 제공하며,
링커가 호출 지점의 심볼을 실제 코드 주소로 연결한다.
CPU는 함수로 이동하고 반환한다. 권한 모드 전환도 시스템콜 번호도 필요 없다.
OpenSBI를 통해 부팅했다는 사실이 콘솔 함수 호출을 시스템콜로 만들지는 않는다.
현재 UART 출력에는 SBI console 호출도 쓰지 않는다.

향후 시스템콜에서는 유저 프로그램이 커널 내부 함수 주소로 직접 호출하지 않는다.
프로그램이 정해진 레지스터에 **번호와 인자**를 넣고 `ecall`을 실행하면,
CPU가 트랩 진입점으로 이동하고 커널이 번호를 해석해서 적절한 함수를 실행한다.
유저 측 함수 이름과 커널 측 함수 이름이 같을 필요는 없다.
둘 사이의 약속은 시스템콜 ABI(번호, 인자 레지스터, 반환 규칙)다.

## 실제 검증 결과 (2026-10-16)

검증 호스트는 macOS `arm64`(Apple Silicon)이며 실제 M1 칩 모델 여부는 별도 확인하지 않았다.
설치된 도구는 Homebrew Clang 20.1.8, QEMU 11.1.0, QEMU 기본 OpenSBI 1.8.1이었다.
독립 Homebrew LLD가 없어 기존 Rust 1.79.0 도구에 포함된 LLD 18.1.7을 사용했다.
검증을 위해 새 패키지를 설치하지 않았다. **실제로 사용한 빌드/테스트 명령**은:

```sh
make test KLD=/Users/hangyeonghyeon/.rustup/toolchains/1.79.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/ld.lld
make run
```

이 환경에서 새로 링크할 때는 위 `KLD=...`를 지정하거나 `brew install lld`를 실행하면 된다.
일반 설치 후에는 `make test`만 실행하면 된다.

검증한 내용:

- `-Wall -Wextra -Werror`를 포함한 freestanding RV64 컴파일과 ELF 링크 성공.
- ELF64 RISC-V, 진입점 `0x80200000`, BSS/스택 LOAD 영역 확인.
- `llvm-nm --undefined-only build/kernel.elf` 출력이 비어 있음: 미해결 외부 함수 없음.
- OpenSBI 로그의 `Domain0 Next Address = 0x80200000`, `Next Mode = S-mode` 확인.
- `make test`: 실제 QEMU에서 총 **16개 검사 통과**. 부팅, help, echo, clear ANSI 바이트,
  빈 echo/행, 공백/탭, 알 수 없는 명령, DEL/Backspace, CRLF, CRLF 다음 명령,
  방향키 무시, 127문자 경계, 초과 행 전체 폐기, 초과 후 복구 확인.
- PTY의 `make run`에서도 `help`, `echo hello`, `clear` 입력 및 Ctrl-a x 종료 확인.

자동 검증의 UART 원본 로그는 `build/smoke.log`에 남는다. 테스트는 프롬프트를 기다린 뒤
입력하고 결과 바이트를 비교하며, 종료 시 자신이 실행한 QEMU만 정리한다.
검증 로그와 ELF는 `make clean` 시 지워진다.

검증하지 못했거나 범위 밖인 내용:

- 깨끗한 Mac에 대한 Homebrew 신규 설치 과정과 독립 Homebrew LLD 조합.
- 실제 보드/STM32에서의 동작, 다중 hart, 다른 QEMU/OpenSBI 버전.
- GUI 터미널의 `clear` 시각 효과. ANSI 출력 바이트는 확인했다.
- 유저 모드, 메모리 보호, 트랩 처리 및 시스템콜: 아직 구현하지 않았다.
- 참고 저장소는 GitHub API의 루트 파일/디렉터리 목록만 확인했다.
  본문 소스 추가 조회는 승인 거절로 진행하지 못했으며, 원본 소스 검토나 호환성 검증을
  했다고 주장하지 않는다. 이 프로젝트는 요청한 첫 단계에 맞춰 독립 작성했다.

## 다음 학습 단계: 아직 구현하지 않은 확장

목표 데이터 흐름은 다음과 같다. 아래 이름은 **향후 구현 예시**다.

```text
유저 모드의 user_write("hello", 5)
  → a7=SYS_WRITE, a0=유저 버퍼 주소, a1=길이 설정
  → ecall
  → S-mode trap_entry (레지스터 저장, 커널 스택으로 전환)
  → trap_handler(): scause가 U-mode ecall(8)인지 확인
  → syscall_dispatch(): a7의 번호로 분기
  → sys_write(): 버퍼/길이/접근 권한 검사
  → console_putc() → uart_putc() → UART 출력
  → 반환값을 저장된 a0에 기록, sepc += 4
  → 레지스터 복원 → sret → 유저 코드의 ecall 다음 명령
```

안전하게 만들어 갈 구현 순서:

1. 먼저 `trap_entry`와 트랩 프레임을 만든다. `stvec`를 설정하고 일반 레지스터,
   `sepc`, `sstatus`를 저장/복원한다. `sscratch` 등을 이용해 유저 스택에서
   별도 커널 스택으로 전환한다. 예상하지 못한 예외는 원인을 출력하고 정지시킨다.
2. 작은 유저 프로그램과 유저 스택을 별도로 배치한다. Sv39 페이지 테이블에서
   유저 코드/데이터/스택만 U 비트를 허용하고 커널과 UART MMIO에는 유저 접근을 막는다.
   현재의 `satp=0`만으로는 S/U 메모리 격리가 되지 않는다.
   `sepc`에 유저 진입 주소, `sstatus.SPP=0`을 설정하고 유저 `sp`를 준비한 뒤
   `sret`으로 U-mode에 진입한다. 프로세스나 스케줄러 없이 한 프로그램부터 시작한다.
3. 프로젝트 자체 ABI를 정한다. 예를 들어 `SYS_WRITE=1`, 번호는 `a7`, 버퍼는 `a0`,
   길이는 `a1`, 반환값은 `a0`로 약속한다. 이 번호는 CPU나 함수 이름이 자동으로
   정하는 값이 아니며 Linux ABI와 같다고 가정하지 않는다.
4. 유저 `user_write()` 래퍼에서 레지스터를 설정하고 `ecall`을 실행한다.
   U-mode ecall이 S-mode로 위임되는지도 OpenSBI의 예외 위임 설정에서 확인한다.
5. `trap_handler()`가 `scause=8`을 확인하고 `syscall_dispatch()`를 호출한다.
   `a7`을 `switch`로 분기해 `sys_write()`를 선택하고, 모르는 번호에는 오류를 반환한다.
6. `sys_write()`가 길이 상한, 주소 덧셈 오버플로, 유저 페이지 권한을 확인하고
   유저 버퍼를 안전하게 읽어 `console_putc()` → `uart_putc()`로 출력한다.
   유저 메모리 읽기는 명시적인 복사 경로나 제한된 SUM 사용과 예외 처리가 필요하다.
   반환값을 트랩 프레임에 기록하고 4바이트 `ecall`을 건너뛰어 `sret`으로 복귀한다.

최종적으로 정상 출력, 알 수 없는 번호, 잘못된 유저 포인터, `ecall` 이후 복귀를 각각
검증하면 일반 함수 호출과 권한 경계를 넘는 시스템콜의 차이를 실제 코드로 볼 수 있다.

## 참고

- 사용자 지정 참고 저장소: [dj258255/kernel-hobby](https://github.com/dj258255/kernel-hobby)
- OpenSBI/virt 부팅: [QEMU virt 공식 문서](https://www.qemu.org/docs/master/system/riscv/virt.html)
- UART 메모리 맵/클록: [QEMU virt 구현](https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c)
- 설치 도구: [Homebrew LLVM](https://formulae.brew.sh/formula/llvm),
  [LLD](https://formulae.brew.sh/formula/lld), [QEMU](https://formulae.brew.sh/formula/qemu)
