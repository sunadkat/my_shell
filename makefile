
CC=gcc

CFLAGS=

all: my_shell


my_shell: my_shell.o


my_shell.o: my_shell.c


clean: ; rm -f *.o $(objects) my_shell
