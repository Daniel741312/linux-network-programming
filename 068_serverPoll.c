#include"033-035_wrap.h"

#define MAXLINE 80
#define SERVER_PORT 9527
#define OPEN_MAX 1024

int main(int argc,char* argv[]){
	int ret=0;
	/*poll函数返回值*/
	int nready=0;
	int i,j,maxi;
	int connectFd,listenFd,socketFd;
	ssize_t n;
	char buf[MAXLINE];
	char str[INET_ADDRSTRLEN];
	socklen_t clientLen;
	/*创建结构体数组*/	
	struct pollfd client[OPEN_MAX];
	/*创建客户端地址结构和服务器地址结构*/
	struct sockaddr_in clientAddr,serverAddr;
	/*得到监听套接字listenFd*/
	listenFd=Socket(AF_INET,SOCK_STREAM,0);
	/*设置地址可复用*/
	int opt=0;
	ret=setsockopt(listenFd,SOL_SOCKET,SO_REUSEADDR,(void*)&opt,sizeof(opt));
	if(ret==-1)
		perr_exit("setsockopt error");
	/*向服务器地址结构填入内容*/
	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);
	/*绑定服务器地址结构到监听套接字,并设置监听上限*/
	Bind(listenFd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));
	Listen(listenFd,128);
	/*初始化第一个pollfd为监听套接字*/
	client[0].fd=listenFd;
	client[0].events=POLLIN;
	/*将pollfd数组的余下内容的fd属性置为-1*/
	for(i=1;i<OPEN_MAX;++i)
		client[i].fd=-1;

	maxi=0;

	while(1){
		/*nready是有多少套接字有POLLIN请求*/
		nready=poll(client,maxi+1,-1);
		if(nready==-1)
			perr_exit("poll error");
		/*如果listenFd的revents有POLLIN请求,则调用Accept函数得到connectFd*/
		if(client[0].revents&POLLIN){
			clientLen=sizeof(clientAddr);
			connectFd=Accept(listenFd,(struct sockaddr*)&clientAddr,&clientLen);
			/*打印客户端地址结构信息*/
			printf("Received from %s at PORT %d\n",inet_ntop(AF_INET,&(clientAddr.sin_addr.s_addr),str,sizeof(str)),ntohs(clientAddr.sin_port));
			/*将创建出来的connectFd加入到pollfd数组中*/
			for(i=1;i<OPEN_MAX;++i)
				if(client[i].fd<0){
					client[i].fd=connectFd;
					break;
				}

			if(i==OPEN_MAX)
				perr_exit("Too many clients,I'm going to die...");
			/*当没有错误时,将对应的events设置为POLLIN*/
			client[i].events=POLLIN;

			if(i>maxi)
				maxi=i;
			if(--nready<=0)
				continue;
		}
		/*开始从1遍历pollfd数组*/
		for(i=1;i<=maxi;++i){
			/*到结尾了或者有异常*/
			if((socketFd=client[i].fd)<0)
				continue;
			/*第i个客户端有连接请求,进行处理*/
			if(client[i].revents&POLLIN){
				if((n=read(socketFd,buf,sizeof(buf)))<0){
					/*出错时进一步判断errno*/
					if(errno=ECONNRESET){
						printf("client[%d] aborted connection\n",i);
						close(socketFd);
						client[i].fd=-1;
					}else
						perr_exit("read error");
				}else if(n==0){
					/*read返回0,说明读到了结尾,关闭连接*/
					printf("client[%d] closed connection\n",i);
					close(socketFd);
					client[i].fd=-1;
				}else{
					/*数据处理*/
					for(j=0;j<n;++j)
						buf[j]=toupper(buf[j]);
					Writen(STDOUT_FILENO,buf,n);
					Writen(socketFd,buf,n);
				}
				if(--nready==0)
					break;
			}
		}
	}
	return 0;
}