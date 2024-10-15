CC = gcc 
CFLAGS = -std=c23 -ggdb3 -O0 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=undefined

# Build server and client
all: server client

# Server build rule
server: src/server.c src/server.h
	$(CC) $(CFLAGS) -o pomobar-server src/server.c src/server.h

# Client build rule
client: src/client.c src/client.h
	$(CC) $(CFLAGS) -o pomobar-client src/client.c src/client.h

# Clean up compiled files
clean:
	rm -f pomobar-server pomobar-client

.PHONY: all clean
