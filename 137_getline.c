#include<sys/socket.h>

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

	if(n==-1){
		i=n;
	}
	return i;
}