#include "wrap.h"

#define SERVER_PORT 9527

struct AddrFd {
	struct sockaddr_in clientaddr;
	int clientfd;
};
typedef struct AddrFd AddrFd;

void* worker(void* arg) {
	AddrFd* p_addr_fd = (AddrFd*)arg;
	char client_ip[128];
	printf("new connection: ip = %s, port = %d\n", inet_ntop(AF_INET, &(p_addr_fd->clientaddr.sin_addr), client_ip, sizeof(client_ip)), htons(p_addr_fd->clientaddr.sin_port));
	char buf[128];
	while (1) {
		ssize_t n = read(p_addr_fd->clientfd, buf, sizeof(buf));
		if (n == 0) {
			printf("connection closed\n");
			break;
		}
		for (int i = 0; i < n; ++i) {
			buf[i] = toupper(buf[i]);
		}
		write(p_addr_fd->clientfd, buf, n);
	}
	close(p_addr_fd->clientfd);
	pthread_exit(0);
}

int main() {
	int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(g_port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	Listen(sockfd, 128);

	pthread_t tid;
	AddrFd addr_fd[128];
	int i = 0;
	while (1) {
		socklen_t clientaddr_len = sizeof(addr_fd[i].clientaddr);
		addr_fd[i].clientfd = Accept(sockfd, (struct sockaddr*)&(addr_fd[i].clientaddr), &clientaddr_len);
		pthread_create(&tid, NULL, worker, (void*)&addr_fd[i]);
		pthread_detach(tid);
		i = (i + 1) % 128;
	}
	close(sockfd);
	return 0;
}