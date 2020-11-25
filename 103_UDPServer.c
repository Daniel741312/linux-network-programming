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

int main(void){
	/*服务器地址结构和客户端地址结构*/
	struct sockaddr_in server_addr,client_addr;
	socklen_t client_addr_len;
	int sockfd;
	ssize_t n=0;
	int i=0;
	char buf[BUFSIZ];
	char str[INET_ADDRSTRLEN];
	/*得到套接字,注意采用报式协议*/
	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	/*初始化服务器地址结构*/
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	server_addr.sin_port=htons(SERVER_PORT);
	/*绑定地址结构到sockfd*/
	bind(sockfd,(const struct sockaddr*)&server_addr,sizeof(server_addr));

	printf("Accepting connection...\n");
	while(1){
		client_addr_len=sizeof(client_addr);
		/*ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,struct sockaddr* src_addr, socklen_t* addrlen);*/
		n=recvfrom(sockfd,buf,BUFSIZ,0,(struct sockaddr*)&client_addr,&client_addr_len);
		if(n==-1)
			perror("recvfrom error");

		printf("Received from %s at port:%d\n",inet_ntop(AF_INET,&client_addr.sin_addr.s_addr,str,sizeof(str)),htons(client_addr.sin_port));

		for(i=0;i<n;++i)
			buf[i]=toupper(buf[i]);
		/*ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,struct sockaddr* src_addr, socklen_t* addrlen);*/
		n=sendto(sockfd,buf,n,0,(struct sockaddr*)&client_addr,sizeof(client_addr));
		if(n==-1)
			perror("sendto error");
	}

	close(sockfd);
	return 0;
}
