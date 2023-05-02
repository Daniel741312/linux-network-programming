#include "wrap.h"

#define MAXLINE 80
#define SERVER_PORT 9527
#define OPEN_MAX 1024

int main(int argc, char* argv[]) {
	char buf[MAXLINE], str[INET_ADDRSTRLEN];
	struct pollfd client[OPEN_MAX];
	struct sockaddr_in clientaddr, serveraddr;
	int listen_fd = Socket(AF_INET, SOCK_STREAM, 0);

	int opt = 0;
	int ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
	if (ret == -1) perr_exit("setsockopt error");

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(g_port);

	Bind(listen_fd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr));
	Listen(listen_fd, 128);

	client[0].fd = listen_fd;
	client[0].events = POLLIN;
	int i = 0, j = 0, maxi = 0;
	for (i = 1; i < OPEN_MAX; ++i)
		client[i].fd = -1;

	while (1) {
		int N = poll(client, maxi + 1, -1);
		if (N == -1)
			perr_exit("poll error");
		if (client[0].revents & POLLIN) {
			socklen_t clientaddr_len = sizeof(clientaddr);
			int connect_fd = Accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddr_len);
			printf("client address: %s, port: %d\n", inet_ntop(AF_INET, &(clientaddr.sin_addr.s_addr), str, sizeof(str)), ntohs(clientaddr.sin_port));
			for (i = 1; i < OPEN_MAX; ++i) {
				if (client[i].fd < 0) {
					client[i].fd = connect_fd;
					break;
				}
			}

			if (i == OPEN_MAX)
				perr_exit("too many clients, dying\n");
			client[i].events = POLLIN;

			if (i > maxi) maxi = i;
			if (--N <= 0) continue;
		}
		for (i = 1; i <= maxi; ++i) {
			int socket_fd = 0, n = 0;
			if ((socket_fd = client[i].fd) < 0)
				continue;
			if (client[i].revents & POLLIN) {
				if ((n = read(socket_fd, buf, sizeof(buf))) < 0) {
					if (errno = ECONNRESET) {
						printf("client[%d] aborted connection\n", i);
						close(socket_fd);
						client[i].fd = -1;
					} else {
						perr_exit("read error");
					}
				} else if (n == 0) {
					printf("client[%d] closed connection\n", i);
					close(socket_fd);
					client[i].fd = -1;
				} else {
					for (j = 0; j < n; ++j)
						buf[j] = toupper(buf[j]);
					write(STDOUT_FILENO, buf, n);
					write(socket_fd, buf, n);
				}
				if (--N == 0)
					break;
			}
		}
	}
	close(listen_fd);
	return 0;
}