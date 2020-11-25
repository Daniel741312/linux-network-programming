/*epoll基于非阻塞IO事件驱动*/
#include<unistd.h>
#include<fcntl.h>
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
#include<sys/epoll.h>

#define MAX_EVENTS 1024
#define BUFLEN 4096
#define SERVER_PORT 9527

struct my_events{
    int fd;                                                     //要监听的文件描述符
    int events;                                                 //对应的监听事件
    void* arg;                                                  //下面回调函数的泛型参数,其实就指向这个结构体本身
    void (*call_back)(int fd,int events,void* arg);             //回调函数
    int status;                                                 //是否在监听:1->在红黑树上,0->不在红黑树上   
    char buf[BUFLEN];
    int len;
    long last_active;                                           //记录每次加入红黑树g_efd的时间值
};

void recvdata(int fd,int event,void* arg);
void senddata(int fd,int event,void* arg);
void eventset(struct my_events* ev,int fd,void(*call_back)(int,int,void*),void* arg);
void eventadd(int fd,int event,struct my_events* ev);
void eventdel(int efd,struct my_events* ev);
void acceptconn(int lfd,int events,void* arg);
void perr_exit(const char* str);
void initlistensocket(int efd,unsigned short port);


int g_efd;                                                      //全局变量:返回epoll_create返回的文件描述符(红黑树树根)
struct my_events g_events[MAX_EVENTS+1];

void recvdata(int fd,int event,void* arg){
    struct my_events* ev=(struct my_events*)arg;
    int len;

    len=recv(fd,ev->buf,sizeof(ev->buf),0);
    eventdel(g_efd,ev);

    if(len>0){
        ev->len=len;
        ev->buf[len]='\0';
        printf("C[%d]:%s",fd,ev->buf);

        eventset(ev,fd,senddata,ev);
        eventadd(g_efd,EPOLLOUT,ev);
    }else if(len==0){
        close(ev->fd);
        printf("fd=%d,pos=%ld closed\n",fd,ev-g_events);
    }else{
        close(ev->fd);
        printf("fd=%d,error=%d:%s\n",fd,errno,strerror(errno));
    }

    return;
}

void senddata(int fd,int event,void* arg){
    struct my_events* ev=(struct my_events*)arg;
    int len;

    len=send(fd,ev->buf,sizeof(ev->buf),0);
    eventdel(g_efd,ev);

    if(len>0){
        printf("send fd=%d,len=%d:%s\n",fd,len,ev->buf);
        eventset(ev,fd,recvdata,ev);
        eventadd(g_efd,EPOLLIN,ev);
    }else{
        close(fd);
        printf("send %d error:%s\n",fd,strerror(errno));
    }

    return;
}

/*将结构体成员变量初始化*/
void eventset(struct my_events* ev,int fd,void(*call_back)(int,int,void*),void* arg){
    ev->fd=fd;
    ev->events=0;
    ev->arg=arg;
    ev->call_back=call_back;
    ev->status=0;
    memset(ev->buf,0,sizeof(ev->buf));
    ev->len=0;
    ev->last_active=time(NULL);

    return;
}

/*向epoll监听的红黑树添加一个文件描述符*/
void eventadd(int efd,int event,struct my_events* ev){
    struct epoll_event epv={0,{0}};
    int op=0;
    epv.data.ptr=ev;
    epv.events=ev->events=event;

    /*没在树上:设置op,并设置在树上*/
    if(ev->status==0){
        op=EPOLL_CTL_ADD;
        ev->status=1;
    }

    if(epoll_ctl(efd,op,ev->fd,&epv)<0)
        printf("event add failed:fd=%d,event=%d",ev->fd,event);
    else
        printf("event add OK:fd=%d,event=%0X,op=%d",ev->fd,event,op);

    return;
}

/*从监听红黑树上摘除节点*/
void eventdel(int efd,struct my_events* ev){
    struct epoll_event epv={0,{0}};

    if(ev->status!=1)
        return;
    
    epv.data.ptr=NULL;
    ev->status=0;
    epoll_ctl(efd,EPOLL_CTL_DEL,ev->fd,&epv);

    return;
}

void acceptconn(int lfd,int events,void* arg){
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen=sizeof(clientAddr);
    int connectFd,i;

    if((connectFd=accept(lfd,(struct sockaddr*)&clientAddr,&clientAddrLen))==-1){
        if((errno!=EAGAIN)&&(errno!=EINTR)){
            /*暂时不做错误处理*/
        }
        perr_exit("accept error");
    }

    do{
        for(i=0;i<MAX_EVENTS;++i)
            if(g_events[i].status==0)
                break;

        if(i==MAX_EVENTS){
            printf("%s:max connections limit[%d]\n",__func__,MAX_EVENTS);
            break;
        }

        int flag=fcntl(connectFd,F_GETFL);
        flag=flag|O_NONBLOCK;
        if(fcntl(connectFd,F_SETFL,flag)==-1)
            perr_exit("fcntl error");

        /*给cfd设置一个my_events结构体,回调函数设置为recvdata*/
        eventset(&g_events[i],connectFd,recvdata,&g_events[i]);
        /*将connectFd加入监听红黑树g_efd中*/
        eventadd(g_efd,EPOLLIN,&g_events[i]);

    }while(0);

    return;
}

void perr_exit(const char* str){
	perror(str);
	exit(1);
}

void initlistensocket(int efd,unsigned short port){
    struct sockaddr_in serverAddr;
    int lfd=0;
    int flg=0;

    lfd=socket(AF_INET,SOCK_STREAM,0);

    flg=fcntl(lfd,F_GETFL);
    flg=flg|O_NONBLOCK;
    fcntl(lfd,F_SETFL,flg);

    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(port);

    bind(lfd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));

    listen(lfd,128);

    /*void eventset(struct my_events* ev,int fd,void(*call_back)(int,int,void*),void* arg);*/
    eventset(&g_events[MAX_EVENTS],lfd,acceptconn,&g_events[MAX_EVENTS]);
    eventadd(efd,EPOLLIN,&g_events[MAX_EVENTS]);

    return;
}

int main(int argc,char* argv[]){
    unsigned short port=SERVER_PORT;
    if(argv==2)
        port=atoi(argv[1]);

    g_efd=epoll_create(MAX_EVENTS+1);
    if(g_efd<0)
        perr_exit("epoll_create error");

    initlistensocket(g_efd,port);
    struct epoll_event events[MAX_EVENTS+1];
    printf("Server running port[%d]\n",port);

    int chekpos=0;
    int i=0;

    while(1){

        /*验证超时,每次测试100个连接,不测试listenFd.当客户端60s没有和服务器通信,则关闭连接*/
        long now=time(NULL);
        for(i=0;i<100;++i,++chekpos){
            if(chekpos==MAX_EVENTS)
                chekpos=0;
            if(g_events[chekpos].status!=1)
                continue;

            long duration=now-g_events[chekpos].last_active;

            if(duration>=60){
                close(g_events[chekpos].fd);
                printf("fd=%d timeout\n",g_events[chekpos].fd);
                eventdel(g_efd,&g_events[chekpos]);
            }
        }

        int nfd=epoll_wait(g_efd,events,MAX_EVENTS+1,1000);
        if(nfd<0){
            printf("epoll_wait error\n");
            break;
        }

        for(i=0;i<nfd;++i){
            struct my_events* ev=(struct my_events*)(events[i].data.ptr);

            /*读就绪事件*/
            if((events[i].events&EPOLLIN)&&(ev->events&EPOLLIN))
                ev->call_back(ev->fd,events[i].events,ev->arg);
            /*写就绪事件*/
            if((events[i].events&EPOLLOUT)&&(ev->events&EPOLLOUT))
                ev->call_back(ev->fd,events[i].events,ev->arg);

        }
        
    }




    return 0;
}