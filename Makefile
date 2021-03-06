CC = gcc
CCFLAGS = -Wall -Wpedantic

# include folders
INCFLAGS = -I.
# libraries
LIBFLAGS = -lm -lpthread
# SDL2 only for client
LIBSDL = -lSDL2 -lSDL2_image -lSDL2_ttf

# obj files and binaries output directories
OBJ_DIR = ./obj
BIN_DIR = ./bin

# Common files to client and server 
COMMON_SRC = $(wildcard ./common/*.c)
COMMON_INC = $(wildcard ./common/*.h)
COMMON_OBJ = $(patsubst ./common/%.c, $(OBJ_DIR)/common/%.o, $(COMMON_SRC))

# client target files
CLIENT_SRC = $(wildcard ./client/*.c)
CLIENT_INC = $(wildcard ./client/*.h)
CLIENT_OBJ = $(patsubst ./client/%.c, $(OBJ_DIR)/client/%.o, $(CLIENT_SRC))
CLIENT_TGT = $(BIN_DIR)/client

# server target files
SERVER_SRC = $(wildcard ./server/*.c)
SERVER_INC = $(wildcard ./server/*.h)
SERVER_OBJ = $(patsubst ./server/%.c, $(OBJ_DIR)/server/%.o, $(SERVER_SRC))
SERVER_TGT = $(BIN_DIR)/server

all: release

debug: CCFLAGS += -g
debug: release

release : $(OBJ_DIR) $(BIN_DIR) $(CLIENT_TGT) $(SERVER_TGT)
	
# always execute this target
$(OBJ_DIR): FORCE
	mkdir -p $@
	mkdir -p $@/client
	mkdir -p $@/common
	mkdir -p $@/server

$(BIN_DIR): FORCE
	mkdir -p $@
	cp -R client/assets $@/assets


$(CLIENT_TGT): $(COMMON_OBJ) $(CLIENT_OBJ)
	$(CC) $(CCFLAGS) -o $@ $^ $(LIBFLAGS) $(LIBSDL) 

$(SERVER_TGT): $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) $(CCFLAGS) -o $@ $^ $(LIBFLAGS) 


$(OBJ_DIR)/client/%.o: ./client/%.c
	$(CC) $(CCFLAGS) -o $@ -c $< $(INCFLAGS) $(LIBFLAGS)

$(OBJ_DIR)/common/%.o: ./common/%.c
	$(CC) $(CCFLAGS) -o $@ -c $< $(INCFLAGS) 

$(OBJ_DIR)/server/%.o: ./server/%.c
	$(CC) $(CCFLAGS) -o $@ -c $< $(INCFLAGS) $(LIBFLAGS)


.depend:
	$(CC) -MM $(CLIENT_SRC) > $@

-include .depends

clean:
	rm -rf $(BIN_DIR)/* 
	rm -rf $(OBJ_DIR)/*

.PHONY: all clean

FORCE: ;