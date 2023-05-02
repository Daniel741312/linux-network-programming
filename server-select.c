#include "wrap.h"

#define SERVER_PORT 9527
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

int main(int argc, char *argv[]) {
	int i, j, n, maxi, maxfd;
	int client[FD_SETSIZE];
	char buf[BUFSIZ], str[INET_ADDRSTRLEN];

	struct sockaddr_in serveraddr, clientaddr;
	socklen_t clientaddr_len;

	int listen_fd = Socket(AF_INET, SOCK_STREAM, 0);

	fd_set rset, allset;
	FD_ZERO(&allset);
	FD_SET(listen_fd, &allset);

	/*Set the address can be reused*/
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(g_port);

	Bind(listen_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	Listen(listen_fd, 128);

	maxfd = listen_fd;
	maxi = -1;

	for (i = 0; i < FD_SETSIZE; ++i)
		client[i] = -1;

	while (1) {
		/*Initialize the rset by allset*/
		rset = allset;
		int N = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (N == -1)
			perr_exit("select error");

		/*If there is a new connection request*/
		if (FD_ISSET(listen_fd, &rset)) {
			clientaddr_len = sizeof(clientaddr);
			int connect_fd = Accept(listen_fd, (struct sockaddr *)&clientaddr, &clientaddr_len);
			printf("client address: %s port: %d\n", inet_ntop(AF_INET, &(clientaddr.sin_addr.s_addr), str, sizeof(str)), ntohs(clientaddr.sin_port));

			for (i = 0; i < FD_SETSIZE; ++i) {
				if (client[i] < 0) {
					client[i] = connect_fd;
					break;
				}
			}

			if (i == FD_SETSIZE) {
				fputs("too many clients\n", stderr);
				exit(1);
			}

			FD_SET(connect_fd, &allset);

			/*Renewal the maxfd*/
			maxfd = MAX(maxfd, connect_fd);
			maxi= MAX(maxi, i); 

			/*Only the connection request,no data read request(no connect_fd is valid)*/
			if(--N == 0) continue;
		}
		int socket_fd = 0;
		/*There is data read request(s),traverse the fd set to corresponding them*/
		for (i = 0; i <= maxi; ++i) {
			if ((socket_fd = client[i]) < 0)
				continue;

			if (FD_ISSET(socket_fd, &rset)) {
				if ((n = read(socket_fd, buf, sizeof(buf))) == 0) {
					close(socket_fd);
					FD_CLR(socket_fd, &allset);
					client[i] = -1;
				} else if (n > 0) {
					for (j = 0; j < n; ++j)
						buf[j] = toupper(buf[j]);
					write(socket_fd, buf, n);
					//write(STDOUT_FILENO, buf, n);
				}
				if (--N == 0) break;
			}
		}
	}
	close(listen_fd);
	return 0;
}
