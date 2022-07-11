OBJ = avl.c queue.c imp.c
path = "./"
all:
	gcc $(OBJ) -o imp
$(OBJ):
.PHONY: clean
clean:
	rm -f *.o imp *.txt
bigfile:
	truncate -s 32M $(path)bigfile32
	truncate -s 128M $(path)bigfile128
	truncate -s 512M $(path)bigfile512
	truncate -s 1024M $(path)bigfile1024
bigfilerm:
	rm $(path)bigfile*
