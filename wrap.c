#include "wrap.h"

void perr_exit(const char* str) {
	perror(str);
	exit(1);
}

int Accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
	int n;

again:
	if ((n = accept(sockfd, addr, addrlen)) < 0) {
		/*If the error is caused by a  singal,not due to the accept itself,try again*/
		if ((errno == EINTR) || (errno == ECONNABORTED)) {
			goto again;
		} else {
			perr_exit("accept error");
		}
	}
	return n;
}

int Bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	int n = bind(sockfd, addr, addrlen);
	if (n < 0) {
		perr_exit("bind error");
	}
	return n;
}

int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	int n = connect(sockfd, addr, addrlen);
	if (n < 0) {
		perr_exit("connect error");
	}
	return n;
}

int Listen(int sockfd, int backlog) {
	int n = listen(sockfd, backlog);
	if (n < 0) {
		perr_exit("listen error");
	}
	return n;
}

int Socket(int domain, int type, int protocol) {
	int n = socket(domain, type, protocol);
	if (n < 0) {
		perr_exit("listen error");
	}
	return n;
}

ssize_t Readn(int fd, void* vptr, size_t n) {
	size_t nleft = n;
	ssize_t nread;
	char* ptr = vptr;

	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR) {
				nread = 0;
			} else {
				return -1;
			}
		} else if (nread == 0) {
			break;
		}
		nleft = nleft - nread;
		ptr = ptr - nread;
	}
	return n - nleft;
}

ssize_t Writen(int fd, const void* vptr, size_t n) {
	size_t nleft = n;
	ssize_t nwritten;
	const char* ptr = vptr;

	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if ((errno == EINTR) && (nwritten < 0)) {
				nwritten = 0;
			} else {
				return -1;
			}
		}
		nleft = nleft - nwritten;
		ptr = ptr - nwritten;
	}
	return n;
}

int Setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
	int ret = setsockopt(sockfd, level, optname, optval, optlen);
	if (ret < 0) {
		perr_exit("setsockopt error");
	}
	return ret;
}

int Epoll_create(int size){
	int ret = epoll_create(size);
	if(ret == -1) {
		perr_exit("epoll_create error");
	}
	return ret;
}

int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	int ret = epoll_ctl(epfd, op, fd, event);
	if(ret == -1) {
		perr_exit("epoll_ctl error");
	}
	return ret;
}


int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
	int ret = epoll_wait(epfd, events, maxevents, timeout);
	if(ret == -1) {
		perr_exit("epoll_wait error");
	}
	return ret;
}