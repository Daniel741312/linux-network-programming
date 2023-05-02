/*epoll基于非阻塞IO事件驱动*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENTS 1024
#define BUFLEN 4096
#define SERVER_PORT 9527

struct my_event {
    int fd;      // 要监听的文件描述符
    int events;  // 对应的监听事件
    void (*call_back)(int fd, int events, void* this);  // 回调函数
    void* this;  // 回调函数的泛型参数,其实就指向这个结构体本身
    int status;  // 是否在监听:1->在红黑树上,0->不在红黑树上
    char buf[BUFLEN];
    int len;
    long last_active;  // 记录每次加入红黑树g_efd的时间值
};
typedef struct my_event my_event;

/*epoll的红黑树树根*/
int g_efd;
my_event g_events[MAX_EVENTS + 1];

void perr_exit(const char* str);

void recv_data(int fd, int event, void* this);
void send_data(int fd, int event, void* this);

void event_set(my_event* event, int fd, void (*call_back)(int, int, void*),
               void* this);
void event_add(int fd, int event, my_event* ev);
void event_del(int efd, my_event* ev);

void accept_connection(int listen_fd, int events, void* this);
void init_listen_socket(int efd, unsigned short port);

int main(int argc, char* argv[]) {
    unsigned short port = g_port;

    g_efd = epoll_create(MAX_EVENTS + 1);
    if (g_efd <= 0) perr_exit("epoll_create error");

    init_listen_socket(g_efd, port);
    struct epoll_event events[MAX_EVENTS + 1];
    int chekpos = 0, i = 0;

    while (1) {
        /*验证超时,每次测试100个连接,不测试listenFd.当客户端60s没有和服务器通信,则关闭连接*/
        long now = time(NULL);
        for (i = 0; i < 100; ++i, ++chekpos) {
            if (chekpos == MAX_EVENTS) chekpos = 0;
            if (g_events[chekpos].status != 1) continue;

            /*时间间隔*/
            long duration = now - g_events[chekpos].last_active;

            if (duration >= 60) {
                close(g_events[chekpos].fd);
                printf("fd=%d timeout\n", g_events[chekpos].fd);
                event_del(g_efd, &g_events[chekpos]);
            }
        }
        // 等待1s，若无结果直接返回
        int N = epoll_wait(g_efd, events, MAX_EVENTS + 1, 1000);
        if (N < 0) {
            perr_exit("epoll_wait error");
        }

        for (i = 0; i < N; ++i) {
            my_event* ev = (my_event*)events[i].data.ptr;

            if ((events[i].events & EPOLLIN) &&
                (ev->events & EPOLLIN)) {  // 读就绪
                ev->call_back(ev->fd, events[i].events, ev->this);
            }
            if ((events[i].events & EPOLLOUT) &&
                (ev->events & EPOLLOUT)) {  // 写就绪
                ev->call_back(ev->fd, events[i].events, ev->this);
            }
        }
    }
    return 0;
}

void perr_exit(const char* str) {
    perror(str);
    exit(1);
}

void recv_data(int fd, int event, void* this) {
    my_event* ev = (my_event*)this;
    int len;

    /*从fd中接收数据到ev的buf中*/
    len = recv(fd, ev->buf, sizeof(ev->buf), 0);
    event_del(g_efd, ev);

    if (len > 0) {
        ev->len = len;
        ev->buf[len] = '\0';
        printf("C[%d]:%s", fd, ev->buf);

        event_set(ev, fd, send_data, ev);
        event_add(g_efd, EPOLLOUT, ev);
    } else if (len == 0) {
        /*recv返回0,说明对端关闭了连接*/
        close(ev->fd);
        printf("fd = %d, pos = %ld closed\n", fd, ev - g_events);
    } else {
        /*返回值<0,出错了*/
        close(ev->fd);
        perr_exit("recv error");
    }
    return;
}

void send_data(int fd, int event, void* this) {
    my_event* ev = (my_event*)this;
    int len;

    /*从ev的buf中发送数据给fd*/
    len = send(fd, ev->buf, sizeof(ev->buf), 0);
    event_del(g_efd, ev);

    if (len > 0) {
        printf("send fd = %d, len = %d:%s\n", fd, len, ev->buf);
        event_set(ev, fd, recv_data, ev);
        event_add(g_efd, EPOLLIN, ev);
    } else {
        close(fd);
        printf("send %d error:%s\n", fd, strerror(errno));
    }
    return;
}

void event_set(my_event* event, int fd, void (*call_back)(int, int, void*),
               void* this) {
    event->fd = fd;
    event->events = 0;
    event->call_back = call_back;
    event->this = this;
    event->status = 0;
    memset(event->buf, 0, sizeof(event->buf));
    event->len = 0;
    event->last_active = time(NULL);

    return;
}

/*向epoll监听红黑树添加一个文件描述符*/
void event_add(int efd, int event, my_event* ev) {
    struct epoll_event epv = {0, {0}};
    int op = 0;
    /*将联合体中的ptr指向传入的my_events实例*/
    epv.data.ptr = ev;
    /*将成员变量events设置为传入的event,ev->events保持相同*/
    epv.events = ev->events = event;

    if (ev->status == 0) {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }
    if (epoll_ctl(efd, op, ev->fd, &epv) < 0) {
        printf("event add failed: fd = %d, event = %d\n", ev->fd, event);
    } else {
        printf("event add OK: fd = %d, event = %0X, op = %d\n", ev->fd, event, op);
    }

    return;
}

/*从epoll监听红黑树上摘除节点*/
void event_del(int efd, my_event* ev) {
    struct epoll_event epv = {0, {0}};

    if (ev->status != 1) return;

    /*联合体中的指针归零,状态归零*/
    epv.data.ptr = NULL;
    ev->status = 0;
    /*将epv从树上摘下来*/
    epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);

    return;
}

void accept_connection(int listen_fd, int events, void* this) {
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len = sizeof(clientaddr);
    int i;
    int connect_fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddr_len);

    if (connect_fd == -1) {
        if ((errno != EAGAIN) && (errno != EINTR)) {
            /*暂时不做错误处理*/
        }
        perr_exit("accept error");
    }

    do {
        for (i = 0; i < MAX_EVENTS; ++i) {
            if (g_events[i].status == 0) break;
        }

        if (i == MAX_EVENTS) {
            printf("%s: max connections limit[%d]\n", __func__, MAX_EVENTS);
            break;
        }

        int flag = fcntl(connect_fd, F_GETFL);
        flag = flag | O_NONBLOCK;
        fcntl(connect_fd, F_SETFL, flag);

        event_set(&g_events[i], connect_fd, recv_data, &g_events[i]);
        event_add(g_efd, EPOLLIN, &g_events[i]);

    } while (0);

    return;
}

void init_listen_socket(int efd, unsigned short port) {
    struct sockaddr_in serveraddr;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int flag = fcntl(listen_fd, F_GETFL);
    flag = flag | O_NONBLOCK;
    fcntl(listen_fd, F_SETFL, flag);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    bind(listen_fd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr));
    listen(listen_fd, 128);

    /*把g_events数组的最后一个元素设置为listen_fd,回调函数设置为accept_connection*/
    event_set(&g_events[MAX_EVENTS], listen_fd, accept_connection,
              &g_events[MAX_EVENTS]);
    event_add(efd, EPOLLIN, &g_events[MAX_EVENTS]);

    return;
}
