#include "wrap.h"

#define SERVER_PORT 9527
#define MAXLINE 80
#define OPEN_MAX 1024

int main(int argc, char* argv[]) {
    int i = 0;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];

    struct sockaddr_in serveraddr, clientaddr;

    int listen_fd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(g_port);

    Bind(listen_fd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr));
    Listen(listen_fd, 128);

    int efd = Epoll_create(OPEN_MAX);

    /*将listenFd加入监听红黑树中*/
    struct epoll_event node;  // 临时节点
    node.events = EPOLLIN;
    node.data.fd = listen_fd;  // 构造临时节点
    int res = Epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &node);

    struct epoll_event ep[OPEN_MAX];  // 监听数组
    while (1) {
        int N = Epoll_wait(efd, ep, OPEN_MAX, -1);  // 阻塞监听

        for (i = 0; i < N; ++i) {
            if (!(ep[i].events & EPOLLIN)) continue;

            if (ep[i].data.fd == listen_fd) {		//建立连接请求
                socklen_t clientaddr_len = sizeof(clientaddr);
                int connect_fd = Accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddr_len);

                node.events = EPOLLIN;
                node.data.fd = connect_fd;
                res = Epoll_ctl(efd, EPOLL_CTL_ADD, connect_fd, &node);
            } else {	//数据处理请求
                int socket_fd = ep[i].data.fd;
                int n = read(socket_fd, buf, sizeof(buf));

                if (n == 0) {
                    res = Epoll_ctl(efd, EPOLL_CTL_DEL, socket_fd, NULL);
                    close(socket_fd);
                    printf("client[%d] closed connection\n", socket_fd);
                } else if (n < 0) {
                    perr_exit("read n < 0 error");
                    res = Epoll_ctl(efd, EPOLL_CTL_DEL, socket_fd, NULL);
                    close(socket_fd);
                } else {
                    for (i = 0; i < n; ++i) buf[i] = toupper(buf[i]);
                    write(STDOUT_FILENO, buf, n);
                    write(socket_fd, buf, n);
                }
            }
        }
    }

    close(listen_fd);
    close(efd);
    return 0;
}
