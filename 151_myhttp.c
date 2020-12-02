#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>
#include<dirent.h>
#include<errno.h>
#include<string.h>
#include<ctype.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
//http://127.0.0.1:9527/makefile

#define MAXSIZE 2048
#define SERVER_PORT 9527

int init_listen_fd(int port,int epfd);
void do_accept(int lfd,int epfd);
void send_respond(int cfd,int no,char* disp,char* type,int len);
void epoll_run(int port);

/*没毛病的函数*/
void do_read(int cfd,int epfd);
void send_file(int cfd,const char* filename);
void send_error(int cfd,int status,char* title,char* text);
void perr_exit(const char* str);
void send_respond_head(int cfd,int no,const char* desp,const char* type,long len);
int get_line(int cfd,char* buf,int size);
int hexit(char c);
void decode_str(char* to,char* from);
void encode_str(char* to,int tosize,const char* from);
void send_dir(int cfd,const char* dirname);
void http_request(const char* request,int cfd);
void disconnect(int cfd,int epfd);
const char* get_file_type(const char* name);


int init_listen_fd(int port,int epfd){
	int ret=0;
	int lfd=socket(AF_INET,SOCK_STREAM,0);
	if(lfd==-1)
		perr_exit("socket error");

	struct sockaddr_in serverAddr;
	memset(&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serverAddr.sin_port=htons(SERVER_PORT);

	/*设置端口复用*/
	int opt=1;
	setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	/*给lfd绑定地址结构*/
	ret=bind(lfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
	if(ret==-1)
		perr_exit("bind error");

	/*设置监听上限*/
	ret=listen(lfd,128);
	if(ret==-1)
		perr_exit("listen error");

	/*将lfd挂到监听红黑树*/
	struct epoll_event ev;
	ev.events=EPOLLIN;
	ev.data.fd=lfd;

	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
	if(ret==-1)
		perr_exit("epoll_ctl error");

	return lfd;
} 

void do_accept(int lfd,int epfd){
	int cfd=0;
	int flag=0;
	int ret=0;

	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen=sizeof(clientAddr);

	cfd=accept(lfd,(struct sockaddr*)&clientAddr,&clientAddrLen);
	if(cfd==-1)
		perr_exit("accept error");
	
	char client_ip[64]={0};
	printf("New Client IP=%s,Port=%d,cfd=%d\n",
			inet_ntop(AF_INET,&clientAddr.sin_addr.s_addr,client_ip,sizeof(client_ip)),
			ntohs(clientAddr.sin_port),
			cfd);
	
	/*设置非阻塞*/
	flag=fcntl(cfd,F_GETFL);
	flag=flag|O_NONBLOCK;
	fcntl(cfd,F_SETFL,flag);

	struct epoll_event ev;
	ev.data.fd=cfd;

	/*设置非阻塞*/
	ev.events=EPOLLIN|EPOLLET;
	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
	if(ret==-1)
		perr_exit("epoll_ctl add cfd error");

	return;
}

void send_respond(int cfd,int no,char* disp,char* type,int len){
	char buf[1024]={0};

	sprintf(buf,"HTTP/1.1 %d %s\r\n",no,disp);
	sprintf(buf+strlen(buf),"%s\r\n",type);
	sprintf(buf+strlen(buf),"Content-Length:%d\r\n",len);
	sprintf(buf+strlen(buf),"\r\n");

	send(cfd,buf,strlen(buf),0);

	return;
}

void epoll_run(int port){
	int i=0;
	int ret=0;
	struct epoll_event all_events[MAXSIZE];

	/*创建监听红黑树树根节点*/
	int epfd=epoll_create(MAXSIZE);
	if(epfd==-1){
		perror("epoll_create error");
		exit(1);
	}

	/*创建lfd,并添加至监听树*/
	int lfd=init_listen_fd(port,epfd);

	while(1){
		ret=epoll_wait(epfd,all_events,MAXSIZE,-1);
		if(ret==-1)
			perr_exit("epoll_wait error");
		
		for(i=0;i<ret;++i){
			/*只处理读事件*/
			struct epoll_event* pev=all_events+i;

			if(!(pev->events&EPOLLIN))
				continue;
			
			/*处理连接请求*/
			if(pev->data.fd==lfd)
				do_accept(lfd,epfd);
			/*读数据*/
			else
				do_read(pev->data.fd,epfd);
		}
	}

	return;
}

/*----OK----*/
void do_read(int cfd,int epfd){
	/*读取一行http协议,拆分,获取get文件名和协议号*/
	char line[1024]={0};
	int len=get_line(cfd,line,sizeof(line));
	if(len==0){
		printf("Client close\n");
		disconnect(cfd,epfd);
	}else{
		printf("----请求头----\n");
		printf("请求行数据:%s\n",line);
		/*清除多余数据,不让他们拥塞缓冲区*/
		while(1){
			char buf[1024]={0};
			len=get_line(cfd,buf,sizeof(buf));
			if(buf[0]=='\n'){
				break;
			}else if(len==-1){
				break;
			}
		}
		printf("----请求尾----\n");
	}

	/*确定是GET方法(忽略大小写比较字符串前n个字符)*/
	if(strncasecmp("get",line,3)==0){
		http_request(line,cfd);
	}
	disconnect(cfd,epfd);
	return;
}

/*发送服务器本地文件给浏览器-OK*/
void send_file(int cfd,const char* filename){
	int n=0;
	int ret=0;
	int fd=0;
	char buf[BUFSIZ]={0};

	fd=open(filename,O_RDONLY);
	if(fd==-1){
		send_error(cfd,404,"Not Found","No such file or direntry");
		exit(1);
	}

	while((n=read(fd,buf,sizeof(buf)))>0){
		ret=send(cfd,buf,n,0);
		if(ret==-1){
			if(errno==EAGAIN){
				perror("send error:");
				continue;
			}else if(errno==EINTR){
				perror("send error:");
				continue;
			}else{
				perror("send error:");
				exit(1);
			}
		}
	}
	if(n==-1)
		perr_exit("read file error");
	close(fd);
	return;
}

/*发送404页面*/
void send_error(int cfd,int status,char* title,char* text){
	char buf[BUFSIZ]={0};

	sprintf(buf,"%s %d %s\r\n","HTTP/1.1",status,title);
	sprintf(buf+strlen(buf),"Content-Type:%s\r\n","text/html");
	sprintf(buf+strlen(buf),"Content-Length:%d\r\n",-1);
	sprintf(buf+strlen(buf),"Contention:close\r\n");
	send(cfd,buf,strlen(buf),0);
	send(cfd,"\r\n",2,0);

	memset(buf,0,BUFSIZ);

	sprintf(buf,"<html><head><title>%d %s</title></head>\n",status,title);
	sprintf(buf+strlen(buf),"<body bgcolor=\"#cc99cc\"><h3 align=\"center\">%d %s</h4>\n",status,title);
	sprintf(buf+strlen(buf),"%s\n",text);
	sprintf(buf+strlen(buf),"<hr>\n</body>\n</html>\n");
	send(cfd,buf,strlen(buf),0);

	return;
}

/*错误处理函数*/
void perr_exit(const char* str){
	perror(str);
	exit(1);
}

/*发送响应头-OK*/
void  send_respond_head(int cfd,int no,const char* desp,const char* type,long len){
	char buf[1024]={0};

	sprintf(buf,"HTTP/1.1 %d %s\r\n",no,desp);
	send(cfd,buf,strlen(buf),0);

	sprintf(buf,"Content-Type:%s\r\n",type);
	sprintf(buf+strlen(buf),"Content-Length:%ld\r\n",len);
	send(cfd,buf,strlen(buf),0);

	send(cfd,"\r\n",2,0);
	return;
}

/*解析http请求消息中每一行的内容*/
int get_line(int cfd,char* buf,int size){
	int i=0;
	char c='\0';
	int n=0;

	while((i<size-1)&&(c!='\n')){
		n=recv(cfd,&c,1,0);
		if(n>0){
			if(c=='\r'){
				/*拷贝读一次*/
				n=recv(cfd,&c,1,MSG_PEEK);
				if((n>0)&&(c=='\n')){
					recv(cfd,&c,1,0);
				}else{
					c='\n';
				}
			}
			buf[i]=c;
			i++;
		}else{
			c='\n';
		}
	}
	buf[i]='\0';

	return i;
}

/*16进制字符转化为10进制-OK*/
int hexit(char c){
	if(c>='0'&&c<='9')
		return c-'0';
	if(c>='a'&&c<='f')
		return c-'a'+10;
	if(c>='A'&&c<='F')
		return c-'A'+10;

	return 0;
}

/*解码函数-OK*/
void decode_str(char* to,char* from){
	for(;*from!='\0';++to,++from){
		if(from[0]=='%'&&isxdigit(from[1])&&isxdigit(from[2])){
			*to=hexit(from[1])*16+hexit(from[2]);
			from+=2;
		}else{
			*to=*from;
		}
	}
	*to='\0';
	return;
}

/*编码函数-OK*/
void encode_str(char* to,int tosize,const char* from){
	int tolen=0;
	for(tolen=0;(*from!='\0')&&(tolen+4<tosize);++from){
		if(isalnum(*from)||strchr("/_.-~",*from)!=(char*)0){
			*to=*from;
			++to;
			++tolen;
		}else{
			sprintf(to,"%%%02x",(int)*from&0xff);
			to+=3;
			tolen+=3;
		}
	}
	*to='\0';
	return;
}

/*发送目录数据-OK*/
void send_dir(int cfd,const char* dirname){
	int i=0;
	int ret=0;
	int num=0;

	char buf[4096]={0};
	sprintf(buf,"<html><head><title>目录名:%s</title></head>",dirname);
	sprintf(buf+strlen(buf),"<body><h1>当前目录:%s</h1><table>",dirname);

	char enstr[1024]={0};
	char path[1024]={0};

	struct dirent** ptr;
	num=scandir(dirname,&ptr,NULL,alphasort);

	for(i=0;i<num;++i){
		char* name=ptr[i]->d_name;

		sprintf(path,"%s/%s",dirname,name);
		printf("path=%s\n",path);
		struct stat st;
		stat(path,&st);

		/*编码生成Unicode编码:诸如%E5%A7...等*/
		encode_str(enstr,sizeof(enstr),name);

		if(S_ISREG(st.st_mode)){
			sprintf(buf+strlen(buf),"<tr><td><a href=\"%s\">%s</a></td>%ld</td></tr>",enstr,name,(long)st.st_size);
		}else if(S_ISDIR(st.st_mode)){
			sprintf(buf+strlen(buf),"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",enstr,name,(long)st.st_size);
		}

		ret=send(cfd,buf,strlen(buf),0);
		if(ret==-1){
			if(errno==EAGAIN){
				perror("send error:");
				continue;
			}else if(errno==EINTR){
				perror("send error:");
				continue;
			}else{
				perror("send error:");
				exit(1);
			}
		}
		memset(buf,0,sizeof(buf));
	}

	sprintf(buf+strlen(buf),"</table></body></html>");
	send(cfd,buf,strlen(buf),0);

	printf("dir message send OK\n");
	return;
}

/*处理http请求-OK*/
void http_request(const char* request,int cfd){
	/*拆分http请求行*/
	char method[12],path[1024],protocol[12];
	sscanf(request,"%[^ ] %[^ ] %[^ ]",method,path,protocol);
	printf("method=%s,path=%s,protocol=%s\n",method,path,protocol);

	/*解码:将不能识别的中文乱码转换为中文*/
	decode_str(path,path);
	char* file=path+1;

	/*如果没有指定访问的资源,默认显示资源目录中的内容*/
	if(!strcmp(path,"/")){
		file="./";
	}

	/*获取文件属性*/
	struct stat st;
	int ret=0;
	ret=stat(file,&st);

	if(ret==-1){
		send_error(cfd,404,"Not Found","No such file or direntry");
		return;
	}

	if(S_ISDIR(st.st_mode)){
		send_respond_head(cfd,200,"OK",get_file_type(".html"),-1);
		send_dir(cfd,file);
	}
	if(S_ISREG(st.st_mode)){
		/*回发http协议头*/
		send_respond_head(cfd,200,"OK",get_file_type(file),st.st_size);
		send_file(cfd,file);
	}
	return;
}

/*断开连接-OK*/
void disconnect(int cfd,int epfd){
	int ret;
	ret=epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
	if(ret==-1)
		perr_exit("epoll_ctl del error");
	close(cfd);
	return;
}

/*判断文件类型-OK*/
const char* get_file_type(const char* name){
	char* dot;
	dot=strrchr(name,'.');

	if(dot==NULL)
		return "text/plain; charset=utf-8";
	if(strcmp(dot,".html")==0||strcmp(dot,"htm")==0)
		return "text/html; charset=utf-8";
	if(strcmp(dot,".jpg")==0||strcmp(dot,"jpeg")==0)
		return "image/jpeg";
	if(strcmp(dot,".gif")==0)
		return "image/gif";
	if(strcmp(dot,".png")==0)
		return "image/png";
	if(strcmp(dot,".css")==0)
		return "text/css";
	if(strcmp(dot,".wav")==0)
		return "audio/wav";
	if(strcmp(dot,".mp3")==0)
		return "audio/mpeg";
	if(strcmp(dot,".avi")==0)
		return "video/x-msvideo";

	return "text/plain; charset=utf-8";
}

int main(int argc,char* argv[]){
	/*从cmd参数中获取端口和server提供的目录*/
	if(argc<3)
		printf("Please input in this format:./server port path\n");
	
	/*用户输入的端口*/
	int port=atoi(argv[1]);

	/*切换进程的工作目录*/
	int ret=chdir(argv[2]);
	if(ret!=0){
		perror("chdir error");
		exit(1);
	}

	/*启动epoll监听*/
	epoll_run(port);

	return 0;
}
