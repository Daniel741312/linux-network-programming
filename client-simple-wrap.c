#include "wrap.h"

#define SERVER_PORT 9527

int main() {
	int clientfd = Socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(g_port);
	int server_ip;
	inet_pton(AF_INET, "192.168.93.11", &server_ip);
	serveraddr.sin_addr.s_addr = server_ip;
	Connect(clientfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	const char* str = "hello, world\n";
	char buf[128];
	for (int i = 0; i < 50; ++i) {
		write(clientfd, str, strlen(str));
		ssize_t n = read(clientfd, buf, sizeof(buf));
		write(STDOUT_FILENO, buf, n);
		sleep(1);
	}
	close(clientfd);
	return 0;
}