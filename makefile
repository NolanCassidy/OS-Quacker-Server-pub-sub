all: p2

p2: p2.o
	gcc -pthread -o p2 p2.o

p2.o:p2.c
	gcc -c p2.c

clean:
	rm -rf *o p2
