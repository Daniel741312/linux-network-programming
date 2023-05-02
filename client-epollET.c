#include "wrap.h"

#define SERVER_PORT 9527
#define MAXLINE 10

int main(int argc, char* argv[]) {
    struct sockaddr_in serveraddr;
    char buf[MAXLINE];
    char ch = 'a';

    int connect_fd = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(g_port);
    inet_pton(AF_INET, "192.168.93.11", &serveraddr.sin_addr);

    Connect(connect_fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    int i = 0;
    while (1) {
        for (i = 0; i < (MAXLINE >> 1); ++i) buf[i] = ch;
        buf[i - 1] = '\n';
        ch++;

        for (; i < MAXLINE; ++i) buf[i] = ch;
        buf[i - 1] = '\n';
        ch++;

        write(connect_fd, buf, sizeof(buf));
        sleep(3);
    }
    close(connect_fd);
    return 0;
}
