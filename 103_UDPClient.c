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

int main(int argc,char* argv[]){
	struct sockaddr_in serverAddr;
	int sockfd,n;
	char buf[BUFSIZ];
	/*获取套接字*/
	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	/*初始化服务器地址结构*/
	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(SERVER_PORT);
	inet_pton(AF_INET,"127.0.0.1",&serverAddr.sin_addr);
	/*不必bind*/
	//bind(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
	while(fgets(buf,BUFSIZ,stdin)!=NULL){
        /*发送给服务器*/
		n=sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
		if(n==-1)
			perr_exit("sendto error");
		/*从服务器收回数据*/
		n=recvfrom(sockfd,buf,BUFSIZ,0,NULL,0);
		if(n==-1)
			perr_exit("recvfrom error");
		/*写到标准输出*/
		write(STDOUT_FILENO,buf,n);
	}
	close(sockfd);
	return 0;
}