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
	//创建监听套接字
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perr_exit("socket error");
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(g_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//绑定地址结构
	int ret = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret == -1) {
		perr_exit("bind error");
	}

	ret = listen(sockfd, 128);
	if (ret == -1) {
		perr_exit("listen error");
	}

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	//阻塞监听
	int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
	if (clientfd == -1) {
		perr_exit("accept error");
	}
	char client_ip[128];
	printf("client ip = %s, client port = %d\n", inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), client_ip, sizeof(client_ip)), ntohs(client_addr.sin_port));
	char buf[128];
	while (1) {
		ssize_t len = read(clientfd, buf, sizeof(buf));
		for (int i = 0; i < len; ++i) {
			buf[i] = toupper(buf[i]);
		}
		write(clientfd, buf, len);
	}
	close(sockfd);
	close(clientfd);
	return 0;
}