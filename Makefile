CC=gcc
CFLAGS=-W -Wall -ansi -pedantic
LDFLAGS=
EXEC=main

all: $(EXEC)

main:  Post.o Upds.o Fa.o Transition.o State.o main.o
	$(CC) -o $@ $^ $(LDFLAGS)

Post.o: Post.c
		$(CC) -o $@ -c $< $(CFLAGS)

Upds.o: Upds.c
			$(CC) -o $@ -c $< $(CFLAGS)

Fa.o: Fa.c
			$(CC) -o $@ -c $< $(CFLAGS)

State.o: State.c
	$(CC) -o $@ -c $< $(CFLAGS)

Transition.o: Transition.c
		$(CC) -o $@ -c $< $(CFLAGS)



main.o: main.c Fa.h Upds.h Post.h
	$(CC) -o $@ -g -c $< $(CFLAGS)


clean:
	@rm -rf *.o

mrproper: clean
	@rm -rf $(EXEC)
