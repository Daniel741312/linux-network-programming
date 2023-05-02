#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/event.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void perr_exit(const char* str) {
    perror(str);
    exit(1);
}

void read_cb(evutil_socket_t fd, short what, void* arg) {
    char buf[BUFSIZ] = {0};
    read(fd, buf, sizeof(buf));

    printf("Read from writer:%s\n", buf);
    printf("what=%s\n", what & EV_READ ? "Yes" : "No");
    sleep(1);

    return;
}

int main(int argc, char* argv[]) {
    int ret = 0;
    int fd = 0;

    unlink("myfifo");
    mkfifo("myfifo", 0644);

    fd = open("myfifo", O_RDONLY | O_NONBLOCK);
    if (fd == -1)
        perr_exit("open error");

    struct event_base* base = event_base_new();

    struct event* ev = NULL;
    ev = event_new(base, fd, EV_READ | EV_PERSIST, read_cb, NULL);

    event_add(ev, NULL);

    event_base_dispatch(base);

    event_base_free(base);
    close(fd);
    return 0;
}