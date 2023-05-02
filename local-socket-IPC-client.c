#include "wrap.h"

#define SERVER_ADDR "server.socket"
#define CLIENT_ADDR "client.socket"

int main(void) {
	int len;
	struct sockaddr_un clientaddr, serveraddr;
	char buf[BUFSIZ];

	int connect_fd = Socket(AF_UNIX, SOCK_STREAM, 0);

	/*绑定客户端的地址结构*/
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strcpy(clientaddr.sun_path, CLIENT_ADDR);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(clientaddr.sun_path);
	unlink(CLIENT_ADDR);
	Bind(connect_fd, (struct sockaddr*)&clientaddr, len);

	/*服务器的地址结构也要绑定*/
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, SERVER_ADDR);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(serveraddr.sun_path);

	Connect(connect_fd, (struct sockaddr*)&serveraddr, len);

	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		write(connect_fd, buf, strlen(buf));
		len = read(connect_fd, buf, sizeof(buf));
		write(STDOUT_FILENO, buf, len);
	}
	close(connect_fd);
	return 0;
}
