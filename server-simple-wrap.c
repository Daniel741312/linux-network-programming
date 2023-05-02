#include "wrap.h"

#define SERVER_PORT 9527

int main() {
	int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(g_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	Listen(sockfd, 128);

	struct sockaddr_in clientaddr;
	socklen_t clientaddr_len;
	int clientfd = Accept(sockfd, (struct sockaddr*)&clientaddr, &clientaddr_len);

	char client_ip[128];
	printf("client ip = %s, client port = %d\n", inet_ntop(AF_INET, &(clientaddr.sin_addr.s_addr), client_ip, sizeof(client_ip)), ntohs(clientaddr.sin_port));

	char buf[128];
	while (1) {
		ssize_t n = read(clientfd, buf, sizeof(buf));
		write(STDOUT_FILENO, buf, n);
		for (int i = 0; i < n; ++i) {
			buf[i] = toupper(buf[i]);
		}
		write(clientfd, buf, n);
		sleep(1);
	}
	close(clientfd);
	close(sockfd);
	return 0;
}