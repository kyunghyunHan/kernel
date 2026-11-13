
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    int pipefd[2];

    // 1. Pipe 생성
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    // 2. 자식 프로세스 생성
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid == 0) {
        // ===== Child Process =====

        close(pipefd[1]);  // 쓰기 FD 닫기

        char buffer[128] = {0};

        // 부모가 보낸 데이터 대기
        ssize_t n = read(pipefd[0],
                         buffer,
                         sizeof(buffer) - 1);

        if (n > 0) {
            buffer[n] = '\0';

            printf("[Child %ld] Received: %s\n",
                   (long)getpid(), buffer);
        } else if (n < 0) {
            perror("child read");
        }

        close(pipefd[0]);

        _exit(n < 0 ? 1 : 0);
    }

    // ===== Parent Process =====

    close(pipefd[0]);  // 읽기 FD 닫기

    const char *message = "Hello from Parent!";

    size_t remaining = strlen(message);
    const char *p = message;
    int failed = 0;

    while (remaining > 0) {
        ssize_t n = write(pipefd[1], p, remaining);

        if (n > 0) {
            p += n;
            remaining -= (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            perror("parent write");
            failed = 1;
            break;
        }
    }

    close(pipefd[1]);

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf("[Parent] Child exit status: %d\n",
               WEXITSTATUS(status));
    }

    return failed ? 1 : 0;
}
