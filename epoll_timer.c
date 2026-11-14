
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <string.h>

#define MAX_EVENTS  4

int main(void)
{
    // 1. epoll 인스턴스 생성
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    if (epfd == -1) {
        perror("epoll_create1");
        return 1;
    }

    // 2. Timer FD 생성 (Non-blocking)
    int tfd = timerfd_create(
        CLOCK_MONOTONIC,
        TFD_NONBLOCK | TFD_CLOEXEC
    );

    if (tfd == -1) {
        perror("timerfd_create");
        close(epfd);
        return 1;
    }

    // 3. 1초 간격 타이머 설정
    struct itimerspec timer = {0};

    timer.it_value.tv_sec = 1;
    timer.it_interval.tv_sec = 1;

    if (timerfd_settime(tfd, 0, &timer, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        close(epfd);
        return 1;
    }

    // 4. stdin을 Non-blocking으로 설정
    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    if (old_flags == -1 ||
        fcntl(STDIN_FILENO, F_SETFL,
              old_flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        close(tfd);
        close(epfd);
        return 1;
    }

    // 5. epoll에 FD 등록
    struct epoll_event ev = {0};

    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                  STDIN_FILENO, &ev) == -1) {
        perror("epoll_ctl stdin");
        goto cleanup;
    }

    ev.data.fd = tfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD,
                  tfd, &ev) == -1) {
        perror("epoll_ctl timer");
        goto cleanup;
    }

    printf("Commands: hello / status / quit\n");
    printf("Timer event every 1 second\n");

    struct epoll_event events[MAX_EVENTS];
    int running = 1;

    while (running) {

        // 6. FD 이벤트가 발생할 때까지 대기
        int n = epoll_wait(
            epfd,
            events,
            MAX_EVENTS,
            -1
        );

        if (n == -1) {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {

            int fd = events[i].data.fd;

            // 7. Timer 이벤트
            if (fd == tfd) {

                uint64_t expirations;

                ssize_t r = read(
                    tfd,
                    &expirations,
                    sizeof(expirations)
                );

                if (r == sizeof(expirations)) {
                    printf("[TIMER] expired %llu time(s)\n",
                           (unsigned long long)expirations);
                }
            }

            // 8. 터미널 입력 이벤트
            else if (fd == STDIN_FILENO) {

                char buf[256];

                ssize_t r = read(
                    STDIN_FILENO,
                    buf,
                    sizeof(buf) - 1
                );

                if (r > 0) {

                    buf[r] = '\0';

                    if (strstr(buf, "quit") != NULL) {
                        printf("[CMD] quitting\n");
                        running = 0;
                    }
                    else if (strstr(buf, "hello") != NULL) {
                        printf("[CMD] Hello Linux!\n");
                    }
                    else if (strstr(buf, "status") != NULL) {
                        printf("[CMD] epoll running\n");
                    }
                    else {
                        printf("[CMD] received: %s", buf);
                    }
                }
                else if (r == 0) {
                    // EOF
                    running = 0;
                }
                else if (errno != EAGAIN &&
                         errno != EWOULDBLOCK &&
                         errno != EINTR) {
                    perror("stdin read");
                    running = 0;
                }
            }
        }
    }

cleanup:
    // 표준 입력의 원래 상태 복원
    fcntl(STDIN_FILENO, F_SETFL, old_flags);

    close(tfd);
    close(epfd);

    return 0;
}
