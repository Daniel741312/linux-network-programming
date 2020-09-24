src=$(wildcard ./*.c)
obj=$(patsubst %.c,%,$(src))

ALL:$(obj)

myArgs=-Wall -g

%:%.c
	gcc  $< -o $@ $(myArgs) 
clean:
	-rm -rf $(obj)
.PHONY:clean ALL
