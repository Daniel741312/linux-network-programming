#include "033-035_wrap.h"

#define SERVER_ADDR "server.socket"
#define CLIENT_ADDR "client.socket"

int main(void){
	int cfd,len;
	struct sockaddr_un clientAddr,serverAddr;
	char buf[BUFSIZ];

	cfd=Socket(AF_UNIX,SOCK_STREAM,0);
	
	/*绑定客户端的地址结构*/
	bzero(&clientAddr,sizeof(clientAddr));
	clientAddr.sun_family=AF_UNIX;
	strcpy(clientAddr.sun_path,CLIENT_ADDR);
	len=offsetof(struct sockaddr_un,sun_path)+strlen(clientAddr.sun_path);
	unlink(CLIENT_ADDR);
	Bind(cfd,(struct sockaddr*)&clientAddr,len);

	/*服务器的地址结构也要绑定*/
	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sun_family=AF_UNIX;
	strcpy(serverAddr.sun_path,SERVER_ADDR);
	len=offsetof(struct sockaddr_un,sun_path)+strlen(serverAddr.sun_path);

	Connect(cfd,(struct sockaddr*)&serverAddr,len);

	while(fgets(buf,sizeof(buf),stdin)!=NULL){
		write(cfd,buf,strlen(buf));
		len=read(cfd,buf,sizeof(buf));
		write(STDOUT_FILENO,buf,len);
	}
	close(cfd);
	return 0;
}

