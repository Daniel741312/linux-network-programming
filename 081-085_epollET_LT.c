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
#include<time.h>
#include<sys/epoll.h>

#define MAX_EVENTS 1024
#define BUFLEN 4096
#define SERVER_PORT 9527

struct my_events{
    int fd;                                                     //要监听的文件描述符
    int events;                                                 //对应的监听事件
    void (*call_back)(int fd,int events,void* arg);             //回调函数
    void* arg;                                                  //回调函数的泛型参数,其实就指向这个结构体本身
    int status;                                                 //是否在监听:1->在红黑树上,0->不在红黑树上   
    char buf[BUFLEN];
    int len;
    long last_active;                                           //记录每次加入红黑树g_efd的时间值
};

/*epoll的红黑树树根(epoll_create返回的文件描述符)*/
int g_efd;
/*数组规模:0~1024共计1025个*/
struct my_events g_events[MAX_EVENTS+1];

//函数声明:
/*错误处理函数*/
void perr_exit(const char* str);

void recvdata(int fd,int event,void* arg);
void senddata(int fd,int event,void* arg);

void eventset(struct my_events* ev,int fd,void (*call_back)(int,int,void*),void* arg);
void eventadd(int fd,int event,struct my_events* ev);
void eventdel(int efd,struct my_events* ev);

void acceptconn(int lfd,int events,void* arg);
/*创建并初始化监听套接字*/
void initlistensocket(int efd,unsigned short port);


//函数实现:
/*错误处理函数*/
void perr_exit(const char* str){
	perror(str);
	exit(1);
}

/*回调函数*/
void recvdata(int fd,int event,void* arg){
    struct my_events* ev=(struct my_events*)arg;
    int len;

    /*从fd中接收数据到ev的buf中*/
    len=recv(fd,ev->buf,sizeof(ev->buf),0);
    eventdel(g_efd,ev);

    if(len>0){
        ev->len=len;
        ev->buf[len]='\0';
        printf("C[%d]:%s",fd,ev->buf);

        eventset(ev,fd,senddata,ev);
        eventadd(g_efd,EPOLLOUT,ev);
    }else if(len==0){
        /*recv返回0,说明对端关闭了连接*/
        close(ev->fd);
        printf("fd=%d,pos=%ld closed\n",fd,ev-g_events);
    }else{
        /*返回值<0,出错了*/
        close(ev->fd);
        printf("fd=%d,error=%d:%s\n",fd,errno,strerror(errno));
    }
    return;
}

/*回调函数*/
void senddata(int fd,int event,void* arg){
    struct my_events* ev=(struct my_events*)arg;
    int len;

    /*从ev的buf中发送数据给fd*/
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

/*设置my_event结构体成员变量*/
void eventset(struct my_events* ev,int fd,void (*call_back)(int,int,void*),void* arg){
    ev->fd=fd;
    ev->events=0;
    ev->call_back=call_back;
    ev->arg=arg;
    ev->status=0;
    memset(ev->buf,0,sizeof(ev->buf));
    ev->len=0;
    ev->last_active=time(NULL);

    return;
}

/*向epoll监听红黑树添加一个文件描述符*/
void eventadd(int efd,int event,struct my_events* ev){
    /*这是系统的epoll_event结构体*/
    struct epoll_event epv={0,{0}};
    int op=0;
    /*将联合体中的ptr指向传入的my_events实例*/
    epv.data.ptr=ev;
    /*将成员变量events设置为传入的event,ev->events保持相同*/
    epv.events=ev->events=event;

    /*没在树上:设置op,并把epv挂在树上*/
    if(ev->status==0){
        op=EPOLL_CTL_ADD;
        ev->status=1;
    }
    if(epoll_ctl(efd,op,ev->fd,&epv)<0)
        printf("event add failed:fd=%d,event=%d\n",ev->fd,event);
    else
        printf("event add OK:fd=%d,event=%0X,op=%d\n",ev->fd,event,op);

    return;
}

/*从epoll监听红黑树上摘除节点*/
void eventdel(int efd,struct my_events* ev){
    struct epoll_event epv={0,{0}};

    if(ev->status!=1)
        return;
    
    /*联合体中的指针归零,状态归零*/
    epv.data.ptr=NULL;
    ev->status=0;
    /*将epv从树上摘下来*/
    epoll_ctl(efd,EPOLL_CTL_DEL,ev->fd,&epv);

    return;
}

/**/
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



/*调用者main:initlistensocket(g_efd,port)*/
void initlistensocket(int efd,unsigned short port){
    struct sockaddr_in serverAddr;
    int lfd=0;
    int flg=0;

    lfd=socket(AF_INET,SOCK_STREAM,0);

    /*设置lfd非阻塞*/
    flg=fcntl(lfd,F_GETFL);
    flg=flg|O_NONBLOCK;
    fcntl(lfd,F_SETFL,flg);

    /*设置地址结构*/
    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(port);

    bind(lfd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));
    listen(lfd,128);

    /*void eventset(struct my_events* ev,int fd,void(*call_back)(int,int,void*),void* arg);*/
    /*把g_events数组的最后一个元素设置为lfd,回调函数设置为acceptconn*/
    eventset(&g_events[MAX_EVENTS],lfd,acceptconn,&g_events[MAX_EVENTS]);
    /*挂上树*/
    eventadd(efd,EPOLLIN,&g_events[MAX_EVENTS]);

    return;
}

int main(int argc,char* argv[]){
    /*选择默认端口号或指定端口号*/
    unsigned short port=SERVER_PORT;
    if(argc==2)
        port=atoi(argv[1]);

    /*创建红黑树根节点*/
    g_efd=epoll_create(MAX_EVENTS+1);
    if(g_efd<0)
        perr_exit("epoll_create error");

    initlistensocket(g_efd,port);
    /*创建一个系统的epoll_event的数组,与my_events的规模相同*/
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

            /*时间间隔*/
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
