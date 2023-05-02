#include <arpa/inet.h>
#include <ctype.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 9527

/*读回调函数*/
void read_cb(struct bufferevent* bev, void* arg) {
    char buf[1024] = {0};

    bufferevent_read(bev, buf, sizeof(buf));

    printf("Server says:%s\n", buf);

    bufferevent_write(bev, buf, strlen(buf) + 1);

    sleep(1);
    return;
}

/*写回调函数*/
void write_cb(struct bufferevent* bev, void* arg) {
    printf("I'm client's write_cb,I'm usless,:(\n");
    return;
}

/*事件回调函数*/
void event_cb(struct bufferevent* bev, short events, void* arg) {
    if (events & BEV_EVENT_EOF)
        printf("End of file\n");
    else if (events & BEV_EVENT_ERROR)
        printf("Something error\n");
    else if (events & BEV_EVENT_CONNECTED)
        printf("Server connected\n");

    bufferevent_free(bev);
    return;
}

/*读终端的回调函数*/
void read_terminal(evutil_socket_t fd, short what, void* arg) {
    char buf[1024] = {0};
    struct bufferevent* bev = (struct bufferevent*)arg;
    int len = 0;
    len = read(fd, buf, sizeof(buf));

    bufferevent_write(bev, buf, len + 1);

    return;
}

int main(int argc, char* argv[]) {
    int fd = 0;
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    struct event_base* base = NULL;
    base = event_base_new();

    struct bufferevent* bev;
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);
    serverAddr.sin_port = htons(g_port);

    bufferevent_socket_connect(bev, (struct sockaddr*)&serverAddr,
                               sizeof(serverAddr));
    bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);

    bufferevent_enable(bev, EV_READ);

    struct event* ev =
        event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, read_terminal, bev);
    event_add(ev, NULL);

    event_base_dispatch(base);

    event_free(ev);
    event_base_free(base);
    return 0;
}
