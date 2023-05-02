#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SERVER_PORT 9527

void perr_exit(const char* str) {
	perror(str);
	exit(1);
}

int main() {
	int clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if (clientfd < 0) {
		perr_exit("socket error");
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(g_port);
	int server_ip;
	inet_pton(AF_INET, "192.168.93.11", &server_ip);
	server_addr.sin_addr.s_addr = server_ip;

	int ret = connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perr_exit("connect error");
	}
	const char* str = "hello, world\n";
	char buf[128];
	for (int i = 0; i < 50; ++i) {
		write(clientfd, str, strlen(str));
		read(clientfd, buf, sizeof(buf));
		printf("%s\n", buf);
		sleep(1);
	}
	close(clientfd);
	return 0;
}