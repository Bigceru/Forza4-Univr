###################
# VR471650
# Davide Cerullo
# VR472656
# Edoardo Bazzotti
# 08/05/2023
###################
CC = gcc
CFLAGS=-Wall -std=gnu99
INCLUDES=-I./inc

SERVER_SRCS=src/errExit.c src/F4Server.c src/shared_memory.c src/semaphore.c
CLIENT_SRCS=src/errExit.c src/F4Client.c src/shared_memory.c src/semaphore.c
CLIENT_BOT_SRCS=src/errExit.c src/F4ClientBot.c src/shared_memory.c src/semaphore.c

SERVER_OBJS=$(SERVER_SRCS:.c=.o)
CLIENT_OBJS=$(CLIENT_SRCS:.c=.o)
CLIENT_BOT_OBJS=$(CLIENT_BOT_SRCS:.c=.o)

all: F4Server F4Client F4ClientBot

F4Server: $(SERVER_OBJS)
		@echo "Making executable: "$@					# $@ nome della regola (F4Server oppure F4Client)
		@$(CC) $^ -o $@									# $^ sono i prerequisiti del target (SERVER_OBJS)

F4Client: $(CLIENT_OBJS)
		@echo "Making executable: "$@
		@$(CC) $^ -o $@

F4ClientBot: $(CLIENT_BOT_OBJS)
		@echo "Making executable: "$@
		@$(CC) $^ -o $@

.c.o:
		@echo "Compiling: "$<
		@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@		# $< è il nome del primo prerequisito (SERVER_SRCS)

clean: 
	@rm -f src/*.o F4Client F4Server F4ClientBot
	@echo "Removed object files and executables..."
