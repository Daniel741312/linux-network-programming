#include<unistd.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<strings.h>
#include<ctype.h>
#include<signal.h>
#include<poll.h>

#define SERVER_PORT 9527

void perr_exit(const char* str){
	perror(str);
	exit(1);
}

int main(int argc,char* argv[]){
	struct sockaddr_in serverAddr;
	int sockfd,n;
	char buf[BUFSIZ];

	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(SERVER_PORT);
	inet_pton(AF_INET,"127.0.0.1",&serverAddr.sin_addr);

	//bind(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));

	while(fgets(buf,BUFSIZ,stdin)!=NULL){
		n=sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
		if(n==-1)
			perr_exit("sendto error");
		
		n=recvfrom(sockfd,buf,BUFSIZ,0,NULL,0);
		if(n==-1)
			perr_exit("recvfrom error");
		
		write(STDOUT_FILENO,buf,n);
	}

	close(sockfd);
	return 0;
}