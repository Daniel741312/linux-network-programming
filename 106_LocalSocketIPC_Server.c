#include "033-035_wrap.h"

#define SERVER_ADDR "server.socket"

int main(int argc,char* argv[]){
	int lfd,cfd,len,size,i;
	struct sockaddr_un serverAddr,clientAddr;
	char buf[BUFSIZ];

	lfd=Socket(AF_UNIX,SOCK_STREAM,0);

	bzero(&serverAddr,sizeof(serverAddr));
	serverAddr.sun_family=AF_UNIX;
	strcpy(serverAddr.sun_path,SERVER_ADDR);

	len=offsetof(struct sockaddr_un,sun_path)+strlen(serverAddr.sun_path);

	unlink(SERVER_ADDR);
	Bind(lfd,(struct sockaddr*)&serverAddr,len);
	Listen(lfd,4);
	printf("Accepting...\n");

	while(1){
		len=sizeof(clientAddr);

		cfd=Accept(lfd,(struct sockaddr*)&clientAddr,(socklen_t*)&len);

		len-=offsetof(struct sockaddr_un,sun_path);
		clientAddr.sun_path[len]='\0';

		printf("Client bind filename:%s\n",clientAddr.sun_path);

		while((size=read(cfd,buf,sizeof(buf)))>0){
			for(i=0;i<size;++i)
				buf[i]=toupper(buf[i]);
			write(cfd,buf,size);
		}

		close(cfd);
	}
	close(lfd);
	return 0;
}
