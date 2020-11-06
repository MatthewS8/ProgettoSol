CC = gcc
CFLAGS = -Wall -pedantic-errors
LDFLAGS = -lpthread

.PHONY: clean test

all: server client supervisor

server: server.c
	$(CC) $(CFLAGS) server.c -o $@ $(LDFLAGS)

supervisor: supervisor.c
	$(CC) $(CFLAGS) supervisor.c -o $@

client: client.c
	$(CC) $(CFLAGS) client.c -o $@

clean:
	rm server client supervisor *.log

test:
	bash test.sh
