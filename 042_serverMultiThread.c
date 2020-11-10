#include "033-035_wrap.h"
#include<fcntl.h>

#define SERVER_PORT 8000
#define MAXLEN		8192

struct s_info{
	struct sockaddr_in clientAddr;
	int connectFd;
};

void* do_work(void* arg){
	int n=0;
	int i=0;
	struct s_info* ts=(struct s_info*)arg;
	char buf[MAXLEN];
	char str[INET_ADDRSTRLEN];
	while(1){
		n=read(ts->connectFd,buf,MAXLEN);
		if(n==0){
			printf("The client %d is closed\n",ts->connectFd);
			break;
		}

		printf("Recive from %s at port %d\n",inet_ntop(AF_INET,&((ts->clientAddr).sin_addr),str,sizeof(str)),ntohs((ts->clientAddr).sin_port));

		for(i=0;i<n;i++)
			buf[i]=toupper(buf[i]);

		write(STDOUT_FILENO,buf,n);
		write(ts->connectFd,buf,n);
	}

	close(ts->connectFd);
	pthread_exit(0);
}

int main(int argc,char* argv[]){
	struct sockaddr_in serverAddr,clientAddr;
	int i=0;
	socklen_t clientAddrLen;
	int listenFd,connectFd;
	struct s_info ts[256];
	pthread_t tid;

	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);

	listenFd=Socket(AF_INET,SOCK_STREAM,0);

	Bind(listenFd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));

	Listen(listenFd,128);

	printf("Accepting client connect...\n");

	while(1){
		clientAddrLen=sizeof(clientAddr);
		connectFd=Accept(listenFd,(struct sockaddr*)&clientAddr,&clientAddrLen);
		ts[i].clientAddr=clientAddr;
		ts[i].connectFd=connectFd;

		pthread_create(&tid,NULL,do_work,(void*)&ts[i]);
		pthread_detach(tid);

		i++;
	}

	return 0;
}
