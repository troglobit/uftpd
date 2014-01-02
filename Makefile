main: *.c
	gcc -Wall *.c -lpthread -o server
clean:  server
	rm  server
