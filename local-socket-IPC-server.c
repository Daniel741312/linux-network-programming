#include "wrap.h"

#define SERVER_ADDR "server.socket"

int main(int argc, char* argv[]) {
	int size, i;
	struct sockaddr_un serveraddr, clientaddr;
	char buf[BUFSIZ];

	int listen_fd = Socket(AF_UNIX, SOCK_STREAM, 0);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, SERVER_ADDR);

	socklen_t len = offsetof(struct sockaddr_un, sun_path) + strlen(serveraddr.sun_path);

	unlink(SERVER_ADDR);
	Bind(listen_fd, (struct sockaddr*)&serveraddr, len);
	Listen(listen_fd, 4);

	while (1) {
		len = sizeof(clientaddr);

		int connect_fd = Accept(listen_fd, (struct sockaddr*)&clientaddr, (socklen_t*)&len);

		len -= offsetof(struct sockaddr_un, sun_path);
		clientaddr.sun_path[len] = '\0';

		printf("client bind filename:%s\n", clientaddr.sun_path);

		while ((size = read(connect_fd, buf, sizeof(buf))) > 0) {
			for (i = 0; i < size; ++i)
				buf[i] = toupper(buf[i]);
			write(connect_fd, buf, size);
		}

		close(connect_fd);
	}
	close(listen_fd);
	return 0;
}
