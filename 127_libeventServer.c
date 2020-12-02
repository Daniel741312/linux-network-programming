#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<ctype.h>
#include<event2/bufferevent.h>
#include<event2/event.h>
#include<event2/listener.h>

#define SERVER_PORT 9527

/*读回调函数*/
void read_cb(struct bufferevent* bev,void* arg){

	char buf[1024]={0};

	/*使用bufferevnet_read从被包裹的套接字中读数据到buf*/
	bufferevent_read(bev,buf,sizeof(buf));
	printf("Client says:%s\n",buf);

	char* p="fuckyou\n";

	/*给客户端回应*/
	bufferevent_write(bev,p,strlen(p)+1);
	sleep(1);

	return;
}

/*写回调函数,向buffer中写完后调用此函数打印提示信息*/
void write_cb(struct bufferevent* bev,void* arg){
	printf("I'm server,write to client successfully |:)\n");
	return;
}

/*事件回调函数,处理异常*/
void event_cb(struct bufferevent* bev,short events,void* arg){
	if(events&BEV_EVENT_EOF)
		printf("Connection closed\n");
	else if(events&BEV_EVENT_ERROR)
		printf("Connectiong error\n");

	bufferevent_free(bev);
	printf("bufferevent has been free\n");
	
	return;
}

/*监听器的监听器的回调函数*/
void cb_listener(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr* addr,int len,void* ptr){
	printf("New client connected\n");
	/*把传进来的基事件指针接收下来*/
	struct event_base* base=(struct event_base*)ptr;

	/*创建bufferevnet事件对象*/
	struct bufferevent* bev;
	bev=bufferevent_socket_new(base,fd,BEV_OPT_CLOSE_ON_FREE);

	/*设置上回调函数*/
	bufferevent_setcb(bev,read_cb,write_cb,event_cb,NULL);

	/*设置读缓冲使能*/
	bufferevent_enable(bev,EV_READ);
	return;
}

int main(int argc,char* argv[]){

	/*定义并初始化服务器的地址结构*/
	struct sockaddr_in serverAddr;
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);

	/*创建基事件*/
	struct event_base* base;
	base=event_base_new();

	/*创建监听器,把基事件作为参数传递给他*/
	struct evconnlistener* listener;
	listener=evconnlistener_new_bind(base,cb_listener,base,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,36,(struct sockaddr*)&serverAddr,sizeof(serverAddr));

	/*循环监听*/
	event_base_dispatch(base);

	/*释放资源*/
	evconnlistener_free(listener);
	event_base_free(base);

	return 0;
}
