#include "shell.h"
#include "console.h"
#include <stddef.h>

#define LINE_CAPACITY 128u /* 끝의 NUL 포함. 실제 입력은 최대 127문자. */

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int string_equal(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

/* 정상 입력이면 1, 너무 긴 행을 버렸으면 0. 호스트 read/stdio는 쓰지 않는다. */
static int shell_read_line(char *line)
{
    static int skip_lf; /* CR LF를 Enter 한 번으로 처리. BSS에서 0으로 초기화. */
    size_t length = 0;
    int overflow = 0;
    int escape = 0;

    for (;;) {
        char c = console_getc();
        if (skip_lf) {
            skip_lf = 0;
            if (c == '\n') {
                continue;
            }
        }
        if (c == '\r' || c == '\n') {
            skip_lf = (c == '\r');
            console_putc('\n');
            line[length] = '\0';
            return !overflow;
        }
        /* 방향키 등의 ESC [ ... / ESC O ... 시퀀스를 명령에 섞지 않는다. */
        if (escape == 1) {
            escape = (c == '[' || c == 'O') ? 2 : 0;
            continue;
        }
        if (escape == 2) {
            if (c >= '@' && c <= '~') {
                escape = 0;
            }
            continue;
        }
        if (c == '\033') {
            escape = 1;
            continue;
        }
        /* 초과한 명령의 앞부분이 실행되지 않도록 Enter까지 모두 버린다. */
        if (overflow) {
            continue;
        }
        if (c == '\b' || c == 127) {
            if (length > 0) {
                --length;
                console_write("\b \b");
            }
        } else if ((c >= ' ' && c <= '~') || c == '\t') {
            if (length == LINE_CAPACITY - 1) {
                overflow = 1;
                console_putc('\a');
            } else {
                /* 이 셸은 ASCII 한 줄 입력만 지원. 탭은 공백 하나로 표시. */
                line[length++] = (c == '\t') ? ' ' : c;
                console_putc(line[length - 1]);
            }
        }
    }
}

static void shell_execute(char *line)
{
    while (is_space(*line)) {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    char *command = line;
    while (*line != '\0' && !is_space(*line)) {
        ++line;
    }
    if (*line != '\0') {
        *line++ = '\0'; /* 첫 토큰을 C 문자열로 분리한다. */
    }
    while (is_space(*line)) {
        ++line;
    }
    const char *arguments = line;

    if (string_equal(command, "help")) {
        console_write("help         - show commands\n"
                      "echo [text]  - print text\n"
                      "clear        - clear screen (ANSI)\n");
    } else if (string_equal(command, "echo")) {
        console_write(arguments);
        console_putc('\n');
    } else if (string_equal(command, "clear")) {
        console_clear();
    } else {
        console_write("Unknown command: ");
        console_write(command);
        console_putc('\n');
    }
}

void shell_run(void)
{
    char line[LINE_CAPACITY];
    for (;;) {
        console_write("kernel> ");
        if (shell_read_line(line)) {
            shell_execute(line);
        } else {
            console_write("Input too long (max 127 characters); line discarded.\n");
        }
    }
}
