#include "wrap.h"

#define MAXLINE 10

int main() {
    char buf[MAXLINE], ch = 'a';
    memset(buf, 0, sizeof(buf));

    int pfd[2];
    pipe(pfd);
    pid_t pid = fork();

    int i = 0;
    if (pid == 0) { //child process, write
        close(pfd[0]);
        while (1) {
            //aaaa\n
            for (i = 0; i < (MAXLINE >> 1); ++i) buf[i] = ch;
            buf[i - 1] = '\n';
            ch++;
            //bbbb\n
            for (; i < MAXLINE; ++i) buf[i] = ch;
            buf[i - 1] = '\n';
            ch++;
            //aaaa\nbbbb\n
            write(pfd[1], buf, sizeof(buf));
            sleep(3);
        }
        close(pfd[1]);
    } else if (pid > 0) {   // parent process, read
        struct epoll_event resevent[10];

        close(pfd[1]);
        int efd = epoll_create(10);

        struct epoll_event event;
        //event.events = EPOLLIN | EPOLLET;       // edge triger
        event.events = EPOLLIN;    // level triger, by default
        event.data.fd = pfd[0];
        epoll_ctl(efd, EPOLL_CTL_ADD, pfd[0], &event);

        while (1) {
            int N = epoll_wait(efd, resevent, 10, -1);
            //printf("N = %d\n", N);  //N = 1

            if (resevent[0].data.fd == pfd[0]) {
                int len = read(pfd[0], buf, MAXLINE / 2);
                write(STDOUT_FILENO, buf, len);
            }
        }

        close(pfd[0]);
        close(efd);
    } else {
        perr_exit("fork error");
    }
    return 0;
}
