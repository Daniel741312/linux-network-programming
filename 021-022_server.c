#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<string.h>
#include<arpa/inet.h>
#include<ctype.h>

#define SERV_PORT 9527

/*错误处理函数*/
void sys_err(const char* str){
	perror(str);
	exit(1);
}

int main(){
	int link_fd=0;				//建立连接的socket文件描述符
	int connect_fd=0;			//用于实际通信的socket文件描述符
	int i=0;				//循环从缓冲区读取数据
	int ret=0;				//检查简单的返回值，变量可复用
	char buf[BUFSIZ];
	int num=0;
	char client_IP[1024];
		
	/*服务器端的地址结构*/
	struct sockaddr_in serv_addr;
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(SERV_PORT);
	serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

	/*成功与服务器建立链接的客户端的地址结构*/
	struct sockaddr_in clint_addr;
	socklen_t clint_addr_len=sizeof(clint_addr);


	link_fd=socket(AF_INET,SOCK_STREAM,0);
	if(link_fd==-1)
		sys_err("socket error");

	ret=bind(link_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
	if(ret==-1)
		sys_err("bind error");

	ret=listen(link_fd,128);
	if(ret==-1)
		sys_err("listen error");

	connect_fd=accept(link_fd,(struct sockaddr*)&clint_addr,&clint_addr_len);
	if(connect_fd==-1)
		sys_err("accept error");

	printf("client IP:%s,client port:%d\n",
		inet_ntop(AF_INET,&clint_addr.sin_addr.s_addr,client_IP,sizeof(client_IP)),
		ntohs(clint_addr.sin_port));

	while(1){
		num=read(connect_fd,buf,sizeof(buf));
		write(STDOUT_FILENO,buf,num);
		for(i=0;i<num;i++)
			buf[i]=toupper(buf[i]);
		write(connect_fd,buf,num);
	}
	close(connect_fd);
	close(link_fd);

	return 0;
}

