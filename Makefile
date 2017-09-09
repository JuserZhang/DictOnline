OBJS=client.c server.c

CFLAGS=-Wall -O -g

all:client server
client:client.c
	gcc $(CFLAGS) client.c -o client
server:server.c
	gcc $(CFLAGS) server.c -o server -lpthread -lsqlite3
clear:
	rm client server
