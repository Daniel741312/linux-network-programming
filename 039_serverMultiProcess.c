#include "033-035_wrap.h"
#include<strings.h>
#include<sys/wait.h>

#define SERVER_PORT 9527

void catch_child(int signum){
	while((waitpid(0,NULL,WNOHANG))>0);
	return ;
}

int main(int argc,char* argv){

	int lfd=0;
	lfd=Socket(AF_INET,SOCK_STREAM,0);

	struct sockaddr_in serverAddr;
	bzero(&serverAddr,sizeof(serverAddr));

	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);

	Bind(lfd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));

	Listen(lfd,128);

	int cfd=0;
	char buf[BUFSIZ];
	int ret=0;
	int i=0;
	int clientAddrLen=0;
	struct sockaddr_in clientAddr;
	clientAddrLen=sizeof(clientAddr);
	pid_t pid=0;

	while(1){
		cfd=Accept(lfd,(struct sockaddr*)&clientAddr,&clientAddrLen);
		pid=fork();
		if(pid<0)
			perr_exit("fork error");
		else if(pid==0){
			close(lfd);
			break;
		}else{
			struct sigaction act;
			act.sa_handler=catch_child;
			act.sa_flags=0;
			sigemptyset(&act.sa_mask);

			ret=sigaction(SIGCHLD,&act,NULL);
			if(ret!=0)
				perr_exit("sigaction error");

			close(cfd);
		}

	}

	if(pid==0){
		while(1){
			ret=read(cfd,buf,sizeof(buf));
			if(ret==0){
				close(cfd);
				exit(1);
			}

			for(i=0;i<ret;i++)
				buf[i]=toupper(buf[i]);

			write(cfd,buf,ret);
			write(STDOUT_FILENO,buf,ret);
		}
	}

	return 0;
}
