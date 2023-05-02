#include "wrap.h"
#define SERVER_PORT 9527

int main() {
	int sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in serveraddr;
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(g_port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	char buf[BUFSIZ];
	struct sockaddr_in clientaddr;
	bzero(&clientaddr, sizeof(clientaddr));
	socklen_t clientaddr_len = sizeof(clientaddr);
	while (1) {
		ssize_t n = recvfrom(sockfd, buf, BUFSIZ, 0, (struct sockaddr*)&clientaddr, &clientaddr_len);
		if (n == -1) {
			perr_exit("recvfrom error");
		}
		for (int i = 0; i < n; ++i) {
			buf[i] = toupper(buf[i]);
		}
		n = sendto(sockfd, buf, n, 0, (struct sockaddr*)&clientaddr, clientaddr_len);
		if (n == -1) {
			perr_exit("sendto error");
		}
	}
	close(sockfd);
	return 0;
}
