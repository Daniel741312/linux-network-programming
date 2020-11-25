#include"033-035_wrap.h"

#define SERVER_PORT 9527
#define MAXLINE 10

int main(int argc,char* argv[]){
    struct sockaddr_in serverAddr,clientAddr;
    socklen_t clientAddrLen=0;
    int listenFd,connectFd;
    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];
    int len=0;
    int flag=0;

    listenFd=Socket(AF_INET,SOCK_STREAM,0);

    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddr.sin_port=htons(SERVER_PORT);

    Bind(listenFd,(const struct sockaddr*)&serverAddr,sizeof(serverAddr));
    Listen(listenFd,128);

    struct epoll_event event;
    struct epoll_event resevent[10];
    int res,efd;

    efd=epoll_create(10);
    event.events=EPOLLIN|EPOLLET;
    //event.events=EPOLLIN;

    printf("Accepting connections...\n");
    clientAddrLen=sizeof(clientAddr);
    connectFd=Accept(listenFd,(struct sockaddr*)&clientAddr,&clientAddrLen);
    printf("Received from %s at port %d\n",inet_ntop(AF_INET,&clientAddr.sin_addr.s_addr,str,sizeof(str)),ntohs(clientAddr.sin_port));

    flag=fcntl(connectFd,F_GETFL);
    flag=flag|O_NONBLOCK;
    fcntl(connectFd,F_SETFL,flag);


    event.data.fd=EPOLLIN|EPOLLET;
    epoll_ctl(efd,EPOLL_CTL_ADD,connectFd,&event);

    while(1){
        printf("epoll wait begin\n");
        res=epoll_wait(efd,resevent,10,-1);
        printf("epoll wait end with res=%d\n",res);

        if(resevent[0].data.fd=connectFd){
            while(len=read(connectFd,buf,MAXLINE/2)>0)
                write(STDOUT_FILENO,buf,len);
        }
    }
    return 0;
}