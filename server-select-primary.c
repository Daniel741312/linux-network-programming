#include "wrap.h"

#define SERVER_PORT 9527

int main(int argc, char* argv[]) {
    int listen_fd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    Setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(g_port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    Bind(listen_fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    Listen(listen_fd, 128);

    fd_set rdset, allset;
    FD_ZERO(&allset);
    FD_SET(listen_fd, &allset);
    int maxfd = listen_fd;

    while (1) {
        rdset = allset;
        int N = select(maxfd + 1, &rdset, NULL, NULL, NULL);
        if (N == -1) {
            perr_exit("select error");
        }
        if (FD_ISSET(listen_fd, &rdset)) {
            struct sockaddr_in clientaddr;
            socklen_t clientaddr_len = sizeof(clientaddr);
            int connect_fd = Accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddr_len);
            FD_SET(connect_fd, &allset);
            maxfd = (maxfd > connect_fd) ? maxfd : connect_fd;
            if (N == 1) {  // 只有listen_fd，后续语句不需执行
                continue;
            }
        }
        for (int i = listen_fd + 1; i <= maxfd; ++i) {
            if (FD_ISSET(i, &rdset)) {
                char buf[128];
                ssize_t n = read(i, buf, sizeof(buf));
                if (n == 0) {
                    close(i);
                    printf("connection closed\n");
                    FD_CLR(i, &allset);
                } else if (n > 0) {
                    // write(STDOUT_FILENO, buf, n);
                    for (int j = 0; j < n; ++j) {
                        buf[j] = toupper(buf[j]);
                    }
                    write(i, buf, n);
                }
            }
        }
    }
    close(listen_fd);
    return 0;
}