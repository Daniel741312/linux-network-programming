#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<arpa/inet.h>
#include<string.h>
#include<strings.h>
#include<ctype.h>
#include<signal.h>
#include<poll.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<event2/event.h>

void perr_exit(const char* str){
	perror(str);
	exit(1);
}

void write_cb(evutil_socket_t fd,short what,void* arg){
	char buf[]="hello libevent";
	write(fd,buf,strlen(buf)+1);

	sleep(1);
	return;
}

int main(int argc,char* argv[]){
	int fd=0;

	fd=open("myfifo",O_WRONLY|O_NONBLOCK);
	if(fd==-1)
		perr_exit("open error");


	struct event_base* base=event_base_new();

	struct event* ev=NULL;
	ev=event_new(base,fd,EV_WRITE|EV_PERSIST,write_cb,NULL);

	event_add(ev,NULL);

	event_base_dispatch(base);


	event_base_free(base);
	event_free(ev);
	close(fd);
	return 0;
}

