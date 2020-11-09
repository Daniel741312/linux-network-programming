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

/*Error dipose*/
void sys_err(const char* str){
	perror(str);
	exit(1);
}

int main(){
	int ret=0;
	int client_fd=0;
	int serv_ip_dst=0;
	int cnt=10;
	int num=0;
	char buf[BUFSIZ];


	struct sockaddr_in serv_addr;
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(SERV_PORT);
	inet_pton(AF_INET,"127.0.0.1",(void*)&serv_ip_dst);
	serv_addr.sin_addr.s_addr=serv_ip_dst;
	

	client_fd=socket(AF_INET,SOCK_STREAM,0);
	if(client_fd==-1)
		sys_err("socket error");

	ret=connect(client_fd,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));
	if(ret==-1)
		sys_err("connect error");

	while(--cnt){
		write(client_fd,"fuck you\n",9);
		num=read(client_fd,buf,sizeof(buf));
		write(STDOUT_FILENO,buf,num);
		sleep(1);
	}
	close(client_fd);
	return 0;
}
