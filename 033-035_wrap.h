#ifndef _WRAP_H_
#define _WRAP_H_
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
#include<sys/epoll.h>
#endif

void perr_exit(const char* str);
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int Bind(int sockfd,const struct sockaddr *addr,socklen_t addrlen);
int Connect(int sockfd,const struct sockaddr *addr,socklen_t addrlen);
int Listen(int sockfd, int backlog);
int Socket(int domain,int type,int protocol);
ssize_t Readn(int fd,void* vptr,size_t n);
ssize_t Writen(int fd,const void* vptr,size_t n);
