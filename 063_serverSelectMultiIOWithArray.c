#include "033-035_wrap.h"

#define SERVER_PORT 9527

int main(int argc, char *argv[]){
	int i, j, n, maxi;
	int nready, client[FD_SETSIZE];
	int listenFd, connectFd, maxFd, socketFd;
	char buf[BUFSIZ], str[INET_ADDRSTRLEN];

	struct sockaddr_in serverAddr, clientAddr;
	socklen_t clientAddrLen;

	listenFd = Socket(AF_INET, SOCK_STREAM, 0);

	fd_set rset, allset;
	FD_ZERO(&allset);
	FD_SET(listenFd, &allset);

	/*Set the address can be reused*/
	int opt = 1;
	setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt));

	bzero(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(SERVER_PORT);

	Bind(listenFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	Listen(listenFd, 128);

	maxFd = listenFd;
	maxi = -1;

	for (i = 0; i < FD_SETSIZE; ++i)
		client[i] = -1;

	while (1){
		/*Initialize the rset by allset*/
		rset = allset;
		nready = select(maxFd + 1, &rset, NULL, NULL, NULL);

		if (nready == -1)
			perr_exit("select error");

		/*If there is a new connection request*/
		if (FD_ISSET(listenFd, &rset)){
			clientAddrLen = sizeof(clientAddr);
			connectFd = Accept(listenFd, (struct sockaddr *)&clientAddr, &clientAddrLen);
			printf("Recived from %s at PORT %d\n", inet_ntop(AF_INET, &(clientAddr.sin_addr.s_addr), str, sizeof(str)), ntohs(clientAddr.sin_port));

			for (i = 0; i < FD_SETSIZE; ++i)
				if (client[i] < 0){
					client[i] = connectFd;
					break;
				}

			if(i==FD_SETSIZE){
				fputs("Too many clients\n",stderr);
				exit(1);
			}

			FD_SET(connectFd, &allset);

			/*Renewal the maxfd*/
			if (maxFd < connectFd)
				maxFd = connectFd;

			if(i>maxi)
				maxi=i;

			/*Only the connection request,no data read request(no connectFd is valid)*/
			if (--nready == 0)
				continue;
		}

		/*There is data read request(s),traverse the fd set to corresponding them*/
		for (i = 0; i <= maxi; ++i){
			if((socketFd=client[i])<0)
				continue;

			if (FD_ISSET(socketFd, &rset)){
				if ((n=read(socketFd,buf,sizeof(buf)))==0){
					close(socketFd);
					FD_CLR(socketFd, &allset);
					client[i]=-1;
				}else if(n>0){
					for (j = 0; j < n; ++j)
						buf[j] = toupper(buf[j]);
					write(socketFd, buf, n);
					write(STDOUT_FILENO, buf, n);
				}
				if(--nready==0)
					break;
			}
		}
	}
	close(listenFd);
	return 0;
}
