#include "wrap.h"

#define SERV_PORT 9527

int main(){
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

	/*Call the packaged function,making the code logic more clear */
	client_fd=Socket(AF_INET,SOCK_STREAM,0);
	Connect(client_fd,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));

	while(--cnt){
		write(client_fd,"fuck you\n",9);
		num=read(client_fd,buf,sizeof(buf));
		write(STDOUT_FILENO,buf,num);
		sleep(1);
	}
	close(client_fd);
	return 0;
}
