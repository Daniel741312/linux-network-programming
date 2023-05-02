#include "wrap.h"

#define SERVER_PORT 9527

int main(int argc, char* argv[]) {
	int sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in serveraddr;
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(g_port);
	int serverip;
	inet_pton(AF_INET, "192.168.93.11", &serveraddr.sin_addr.s_addr);

	//Bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	char buf[1024];
	while (fgets(buf, 1024, stdin) != NULL) {
		ssize_t n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		if (n == -1) {
			perr_exit("sendto error");
		}
		n = recvfrom(sockfd, buf, 1024, 0, NULL, 0);
		if (n == -1) {
			perr_exit("recvfrom error");
		}
		write(STDOUT_FILENO, buf, n);
	}
	close(sockfd);
	return 0;
}