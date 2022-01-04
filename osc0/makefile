CC = gcc
LIBS = -lm -lpthread -lrt

all: server client
.PHONY: all

server: server.c
	gcc server.c -o server -lm -lpthread -lrt

client: client.c
	gcc client.c -o client -lm -lpthread -lrt

.PHONY: clean
clean:
	rm server client dph
