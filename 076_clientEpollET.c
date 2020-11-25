#include"033-035_wrap.h"

#define SERVER_PORT 9527
#define MAXLINE 10

int main(int argc,char* argv[]){
    struct sockaddr_in serverAddr;
    char buf[MAXLINE];
    int socketFd,i;
    char ch='a';

    socketFd=Socket(AF_INET,SOCK_STREAM,0);
    bzero(&serverAddr,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(SERVER_PORT);
    inet_pton(AF_INET,"127.0.0.1",&serverAddr.sin_addr);

    Connect(socketFd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));

    while(1){
        for(i=0;i<(MAXLINE>>1);++i)
            buf[i]=ch;
        buf[i-1]='\n';
        ch++;

        for(;i<MAXLINE;++i)
            buf[i]=ch;
        buf[i-1]='\n';
        ch++;

        write(socketFd,buf,sizeof(buf));
        sleep(3);
    }
    close(socketFd);
    return 0;
