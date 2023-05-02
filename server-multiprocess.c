#include "wrap.h"

#define SERVER_PORT 9527

void catch_child(int signum) {
	while (waitpid(0, NULL, WNOHANG) > 0)
		;
}

int main(int argc, char* argv[]) {
	int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(g_port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	Listen(sockfd, 128);

	pid_t pid;
	int clientfd;
	while (1) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddr_len = sizeof(clientaddr);
		clientfd = Accept(sockfd, (struct sockaddr*)&clientaddr, &clientaddr_len);
		pid = fork();
		if (pid < 0) {
			perr_exit("fork error");
		} else if (pid == 0) {
			close(sockfd);
			break;
		} else {
			close(clientfd);
			struct sigaction act;
			act.sa_handler = catch_child;
			act.sa_flags = 0;
			sigemptyset(&act.sa_mask);
			int ret = sigaction(SIGCHLD, &act, NULL);
			if (ret < 0) {
				perr_exit("sigemptyset error");
			}
		}
	}

	//子进程专属执行流
	if (pid == 0) {
		char buf[128];
		while (1) {
			ssize_t n = read(clientfd, buf, sizeof(buf));
			if (n == 0) {
				close(clientfd);
				exit(1);
			}
			for (int i = 0; i < n; ++i) {
				buf[i] = toupper(buf[i]);
			}
			write(clientfd, buf, n);
		}
	}
	return 0;
}