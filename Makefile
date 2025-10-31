CC = gcc# compiler
CFLAGS = -Wall -g# compile flags
LIBS = -lpthread -lrt# libs

all: ks_client ks_server

ks_client: ks_client.o
	$(CC) -o ks_client ks_client.o $(LIBS)

ks_server: ks_server.o
	$(CC) -o ks_server ks_server.o $(LIBS)

%.o:%.c
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f ks_server ks_client *.o *~
