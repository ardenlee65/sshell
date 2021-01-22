
sshell: sshell.o
	gcc sshell.o -o sshell

sshell.o: sshell.c
	gcc -c sshell.c

clean:
	rm*.o sshell
