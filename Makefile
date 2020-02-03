#Compiler flags - from https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html
CFLAGS = -Wall -Werror

sshell : sshell.o
	cc $(CFLAGS) -o sshell sshell.o

sshell.o : sshell.c
	cc $(CFLAGS) -c sshell.c

clean :
	rm sshell sshell.o
