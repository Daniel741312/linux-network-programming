#include "033-035_wrap.h"

#define SERVER_PORT 9527

int main(int argc,char* argv[]){
	int listenFd,connectFd;
	char buf[BUFSIZ];

	struct sockaddr_in serverAddr,clientAddr;
	socklen_t clientAddrLen;

	listenFd=Socket(AF_INET,SOCK_STREAM,0);

	/*Set the address can be reused*/
	int opt=1;
	setsockopt(listenFd,SOL_SOCKET,SO_REUSEADDR,(void*)&opt,sizeof(opt));

	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);

	Bind(listenFd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
	Listen(listenFd,128);

	fd_set rset,allset;
	FD_ZERO(&allset);
	FD_SET(listenFd,&allset);
	int ret=0;
	int maxfd=listenFd;
	int i=0;
	int j=0;
	int n=0;

	while(1){
		/*Initialize the rset by allset*/
		rset=allset;
		ret=select(maxfd+1,&rset,NULL,NULL,NULL);

		if(ret==-1)
			perr_exit("select error");

		/*If there is a new connection request*/
		if(FD_ISSET(listenFd,&rset)){
			clientAddrLen=sizeof(clientAddr);
			connectFd=Accept(listenFd,(struct sockaddr*)&clientAddr,&clientAddrLen);
			/*Add the new connectFd to allset*/
			FD_SET(connectFd,&allset);

			/*Renewal the maxfd*/
			if(maxfd<connectFd)
				maxfd=connectFd;

			/*Only the connection request,no data read request(no connectFd is valid)*/
			if(ret==1)
				continue;
		}

		/*There is data read request(s),traverse the fd set to corresponding them*/
		for(i=listenFd+1;i<=maxfd;++i){
			if(FD_ISSET(i,&rset)){
				n=read(i,buf,sizeof(buf));

				if(n==-1)
					perr_exit("read error");
				else if(n==0){
					close(i);
					FD_CLR(i,&allset);
				}

				for(j=0;j<n;++j)
					buf[j]=toupper(buf[j]);

				write(i,buf,n);
				write(STDOUT_FILENO,buf,n);
			}
		}
	}

	close(listenFd);
	return 0;
}
