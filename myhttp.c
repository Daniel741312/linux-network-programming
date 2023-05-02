#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXSIZE 2048
int g_port = 0;

int init_listen_fd(int port, int epfd);
void do_accept(int lfd, int epfd);
void send_respond(int cfd, int no, char* disp, char* type, int len);
void epoll_run(int port);

/*没毛病的函数*/
void do_read(int cfd, int epfd);
void send_file(int cfd, const char* filename);
void send_error(int cfd, int status, char* title, char* text);
void perr_exit(const char* str);
void send_respond_head(int cfd, int no, const char* desp, const char* type,
                       long len);
int get_line(int cfd, char* buf, int size);
int hexit(char c);
void decode_str(char* to, char* from);
void encode_str(char* to, int tosize, const char* from);
void send_dir(int cfd, const char* dirname);
void http_request(const char* request, int cfd);
void disconnect(int cfd, int epfd);
const char* get_file_type(const char* name);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: ./server <port> <path>\n");
        exit(1);
    }
    g_port = atoi(argv[1]);
    int ret = chdir(argv[2]);	//expect "http/"
    if (ret != 0) {
        perr_exit("chdir error");
	}

    epoll_run(g_port);

    return 0;
}

int init_listen_fd(int port, int epfd) {
    int ret = 0;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perr_exit("socket error");
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(g_port);

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ret = bind(lfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (ret == -1) {
        perr_exit("bind error");
    }

    ret = listen(lfd, 128);
    if (ret == -1) {
        perr_exit("listen error");
	}

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1) {
        perr_exit("epoll_ctl error");
	}

    return lfd;
}

void do_accept(int lfd, int epfd) {
    struct sockaddr_in clientaddr;
    socklen_t clientaddr_len = sizeof(clientaddr);

    int cfd = accept(lfd, (struct sockaddr*)&clientaddr, &clientaddr_len);
    if (cfd == -1) {
        perr_exit("accept error");
	}

    int flag = fcntl(cfd, F_GETFL);
    flag = flag | O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    struct epoll_event ev;
    ev.data.fd = cfd;

    ev.events = EPOLLIN | EPOLLET;	//ET模式
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1) {
        perr_exit("epoll_ctl add cfd error");
	}
}

void send_respond(int cfd, int no, char* disp, char* type, int len) {
    char buf[1024] = {0};

    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
    sprintf(buf + strlen(buf), "%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    sprintf(buf + strlen(buf), "\r\n");

    send(cfd, buf, strlen(buf), 0);

    return;
}

void epoll_run(int port) {
    int i = 0;
    struct epoll_event all_events[MAXSIZE];

    int epfd = epoll_create(MAXSIZE);
    if (epfd == -1) {
		perr_exit("epoll_create error");
    }

    int lfd = init_listen_fd(port, epfd);

    while (1) {
        int N = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if (N == -1) {
            perr_exit("epoll_wait error");
        }

        for (i = 0; i < N; ++i) {
            struct epoll_event* pev = all_events + i;

            if (!(pev->events & EPOLLIN))
                continue;

            if (pev->data.fd == lfd) {
                do_accept(lfd, epfd);
            } else {
                do_read(pev->data.fd, epfd);
            }
        }
    }
}

void do_read(int cfd, int epfd) {
    char line[1024] = {0};
    int len = get_line(cfd, line, sizeof(line));
    if (len == 0) {
        printf("client close\n");
        disconnect(cfd, epfd);
    } else {
        printf("request line: %s\n", line);
        while (1) {
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if (buf[0] == '\n') {
                break;
            } else if (len == -1) {
                break;
            }
        }
    }

    if (strncasecmp("get", line, 3) == 0) {
        http_request(line, cfd);
    }
    disconnect(cfd, epfd);
    return;
}

int get_line(int cfd, char* buf, int size) {
    int i = 0;
    char c = '\0';
    int n = 0;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(cfd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(cfd, &c, 1, MSG_PEEK);	//拷贝读一次
                if ((n > 0) && (c == '\n')) {
                    recv(cfd, &c, 1, 0);	//实际读一次
                } else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    if (n == -1) {
        i = n;
    }
    return i;
}

/*发送服务器本地文件给浏览器-OK*/
void send_file(int cfd, const char* filename) {
    int n = 0;
    int ret = 0;
    int fd = 0;
    char buf[BUFSIZ] = {0};

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        send_error(cfd, 404, "Not Found", "No such file or direntry");
        exit(1);
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ret = send(cfd, buf, n, 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
    }
    if (n == -1)
        perr_exit("read file error");
    close(fd);
    return;
}

/*发送404页面*/
void send_error(int cfd, int status, char* title, char* text) {
    char buf[BUFSIZ] = {0};

    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", "text/html");
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", -1);
    sprintf(buf + strlen(buf), "Contention:close\r\n");
    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);

    memset(buf, 0, BUFSIZ);

    sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
    sprintf(buf + strlen(buf),
            "<body bgcolor=\"#cc99cc\"><h3 align=\"center\">%d %s</h4>\n",
            status, title);
    sprintf(buf + strlen(buf), "%s\n", text);
    sprintf(buf + strlen(buf), "<hr>\n</body>\n</html>\n");
    send(cfd, buf, strlen(buf), 0);

    return;
}

/*错误处理函数*/
void perr_exit(const char* str) {
    perror(str);
    exit(1);
}

/*发送响应头-OK*/
void send_respond_head(int cfd, int no, const char* desp, const char* type,
                       long len) {
    char buf[1024] = {0};

    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, desp);
    send(cfd, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);

    send(cfd, "\r\n", 2, 0);
    return;
}



/*16进制字符转化为10进制-OK*/
int hexit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/*解码函数-OK*/
void decode_str(char* to, char* from) {
    for (; *from != '\0'; ++to, ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
    return;
}

/*编码函数-OK*/
void encode_str(char* to, int tosize, const char* from) {
    int tolen = 0;
    for (tolen = 0; (*from != '\0') && (tolen + 4 < tosize); ++from) {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
    return;
}

/*发送目录数据-OK*/
void send_dir(int cfd, const char* dirname) {
    int i = 0;
    int ret = 0;
    int num = 0;

    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>目录名:%s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>当前目录:%s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    struct dirent** ptr;
    // num=scandir(dirname,&ptr,NULL,alphasort);

    for (i = 0; i < num; ++i) {
        char* name = ptr[i]->d_name;

        sprintf(path, "%s/%s", dirname, name);
        printf("path=%s\n", path);
        struct stat st;
        stat(path, &st);

        /*编码生成Unicode编码:诸如%E5%A7...等*/
        encode_str(enstr, sizeof(enstr), name);

        if (S_ISREG(st.st_mode)) {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td>%ld</td></tr>", enstr,
                    name, (long)st.st_size);
        } else if (S_ISDIR(st.st_mode)) {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }

        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
    }

    sprintf(buf + strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    printf("dir message send OK\n");
    return;
}

void http_request(const char* request, int cfd) {
    char method[12], path[1024], protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    printf("method: %s,path: %s,protocol: %s\n", method, path, protocol);

    decode_str(path, path);
    char* file = path + 1;

    if (!strcmp(path, "/")) {
        file = "./";
    }

    struct stat st;
    int ret = stat(file, &st);

    if (ret == -1) {
        send_error(cfd, 404, "Not Found", "No such file or direntry");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        send_dir(cfd, file);
    }
    if (S_ISREG(st.st_mode)) {
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        send_file(cfd, file);
    }
    return;
}

//epoll delete; close
void disconnect(int cfd, int epfd) {
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if (ret == -1) {
        perr_exit("epoll_ctl del error");
	}
    close(cfd);
}

/*判断文件类型-OK*/
const char* get_file_type(const char* name) {
    char* dot;
    dot = strrchr(name, '.');

    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, "htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, "jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";

    return "text/plain; charset=utf-8";
}
