CC=gcc

problem1:
	$(CC) problem1.c helpers.c -o problem1 -lpthread

problem2:
	$(CC) problem2.c helpers.c -o problem2 -lpthread

all: problem1 problem2

clean : 
	rm problem1 problem2

.PHONY : clean all problem1 problem2