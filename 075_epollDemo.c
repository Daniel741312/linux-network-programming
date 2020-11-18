#include"033-035_wrap.h"

#define MAXLINE 10

int main(){
    int efd,i;
    int pfd[2];
    pid_t pid;
    char buf[MAXLINE],ch='a';

    pipe(pfd);
    pid=fork();
    
    if(pid==0){
        close(pfd[0]);
        while(1){
            for(i=0;i<(MAXLINE>>1);++i)
                buf[i]=ch;
            buf[i-1]='\n';

            ch++;
            for(;i<MAXLINE;++i)
                buf[i]=ch;
            buf[i-1]='\n';
            ch++;

            write(pfd[1],buf,sizeof(buf));
            sleep(3);
        }
        close(pfd[1]);
    }else if(pid>0){
        struct epoll_event event;
        struct epoll_event resevent[10];
        int res,len;

        close(pfd[1]);
        efd=epoll_create(10);

        event.events=EPOLLIN|EPOLLET;
        //event.events=EPOLLIN;
        event.data.fd=pfd[0];
        epoll_ctl(efd,EPOLL_CTL_ADD,pfd[0],&event);

        while(1){
            res=epoll_wait(efd,resevent,10,-1);
            printf("res=%d\n",res);

            if(resevent[0].data.fd==pfd[0]){
                len=read(pfd[0],buf,MAXLINE/2);
                write(STDOUT_FILENO,buf,len);
            }
        }

        close(pfd[0]);
        close(efd);
    }else{
        perr_exit("fork error");
    }
    return 0;
}
