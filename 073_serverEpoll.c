#include "033-035_wrap.h"

#define SERVER_PORT 9527
#define MAXLINE     80
#define OPEN_MAX    1024

int main(int argc,char* argv[]){
    int i=0,n=0,num=0;
    int clientAddrLen=0;
    int listenFd=0,connectFd=0,socketFd=0;
    ssize_t nready,efd,res;
    char buf[MAXLINE],str[INET_ADDRSTRLEN];

    struct sockaddr_in serverAddr,clientAddr;
    /*创建一个临时节点temp和一个数组ep*/
    struct epoll_event temp;
    struct epoll_event ep[OPEN_MAX];

    /*创建监听套接字*/
    listenFd=Socket(AF_INET,SOCK_STREAM,0);
    /*设置地址可复用*/
    int opt=1;
    setsockopt(listenFd,SOL_SOCKET,SO_REUSEADDR,(void*)&opt,sizeof(opt));

    /*初始化服务器地址结构*/
    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(SERVER_PORT);

    /*绑定服务器地址结构*/
    Bind(listenFd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));
    /*设置监听上限*/
    Listen(listenFd,128);

    /*创建监听红黑树*/
    efd=epoll_create(OPEN_MAX);
    if(efd==-1)
        perr_exit("epoll_create error");

    /*将listenFd加入监听红黑树中*/
    temp.events=EPOLLIN;
    temp.data.fd=listenFd;
    res=epoll_ctl(efd,EPOLL_CTL_ADD,listenFd,&temp);
    if(res==-1)
        perr_exit("epoll_ctl error");

    while(1){
        /*阻塞监听写事件*/
        nready=epoll_wait(efd,ep,OPEN_MAX,-1);
        if(nready==-1)
            perr_exit("epoll_wait error");

        /*轮询整个数组(红黑树)*/
        for(i=0;i<nready;++i){
            if(!(ep[i].events&EPOLLIN))
                continue;

            /*如果是建立连接请求*/
            if(ep[i].data.fd==listenFd){
                clientAddrLen=sizeof(clientAddr);
                connectFd=Accept(listenFd,(struct sockaddr*)&clientAddr,&clientAddrLen);
                printf("Received from %s at PORT %d\n",inet_ntop(AF_INET,&clientAddr.sin_addr.s_addr,str,sizeof(str)),ntohs(clientAddr.sin_port));
                printf("connectFd=%d,client[%d]\n",connectFd,++num);

                /*将新创建的连接套接字加入红黑树*/
                temp.events=EPOLLIN;
                temp.data.fd=connectFd;
                res=epoll_ctl(efd,EPOLL_CTL_ADD,connectFd,&temp);
                if(res==-1)
                    perr_exit("epoll_ctl errror");
            }else{
                /*不是建立连接请求,是数据处理请求*/
                socketFd=ep[i].data.fd;
                n=read(socketFd,buf,sizeof(buf));
                
                /*读到0说明客户端关闭*/
                if(n==0){
                    res=epoll_ctl(efd,EPOLL_CTL_DEL,socketFd,NULL);
                    if(res==-1)
                        perr_exit("epoll_ctl error");
                    close(socketFd);
                    printf("client[%d] closed connection\n",socketFd);
                }else if(n<0){
                    /*n<0报错*/
                    perr_exit("read n<0 error");
                    res=epoll_ctl(efd,EPOLL_CTL_DEL,socketFd,NULL);
                    close(socketFd);
                }else{
                    /*数据处理*/
                    for(i=0;i<n;++i)
                        buf[i]=toupper(buf[i]);
                    write(STDOUT_FILENO,buf,n);
                    Writen(socketFd,buf,n);
                }
            }
        }
    }

    close(listenFd);
    close(efd);
    return 0;
}
