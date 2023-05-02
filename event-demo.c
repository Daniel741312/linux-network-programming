#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/event.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int i = 0;
    struct event_base* base = event_base_new();

    const char** buf;
    const char* str;

    buf = event_get_supported_methods();

    str = event_base_get_method(base);
    printf("str=%s\n", str);

    for (i = 0; i < 10; ++i) {
        printf("buf[i]=%s\n", buf[i]);
    }

    return 0;
}