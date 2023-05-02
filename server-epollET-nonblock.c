#include "wrap.h"

#define SERVER_PORT 9527
#define MAXLINE 10

int main(int argc, char* argv[]) {
    struct sockaddr_in serveraddr, clientaddr;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];

    int listen_fd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(g_port);

    Bind(listen_fd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr));
    Listen(listen_fd, 128);

    struct epoll_event event;
    struct epoll_event resevent[10];

    int efd = epoll_create(10);
    event.events = EPOLLIN | EPOLLET;
    // event.events=EPOLLIN;

    socklen_t clientaddr_len = sizeof(clientaddr);
    int connect_fd = Accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddr_len);
    printf("accepting connections...\n");

    int flag = fcntl(connect_fd, F_GETFL);
    flag = flag | O_NONBLOCK;
    fcntl(connect_fd, F_SETFL, flag);

    event.data.fd = connect_fd;
    epoll_ctl(efd, EPOLL_CTL_ADD, connect_fd, &event);

    while (1) {
        int res = epoll_wait(efd, resevent, 10, -1);  // res = 1

        int len = 0;
        if (resevent[0].data.fd = connect_fd) {
            while (len = read(connect_fd, buf, MAXLINE / 2) > 0) {
                write(STDOUT_FILENO, buf, len);
            }
        }
    }
    return 0;
}