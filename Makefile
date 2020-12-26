CC=gcc
CCFLAGS= -Wall -Wpedantic -g

# include folders
INCFLAGS = -I.
# libraries
LIBFLAGS =  -lm -lpthread
# SDL2 only for client
LIBSDL = -lSDL2 -lSDL2_image -lSDL2_ttf

# obj files and binaries output directories
OBJ_DIR = ./obj
BIN_DIR = ./bin

# Common files to client and server 
COMMON_SRC = $(wildcard ./common/*.c)
COMMON_INC = $(wildcard ./common/*.h)
COMMON_OBJ = $(patsubst ./common/%.c, $(OBJ_DIR)/common/%.o, $(COMMON_SRC))

# client target
CLIENT_SRC = $(wildcard ./client/*.c)
CLIENT_INC = $(wildcard ./client/*.h)
CLIENT_OBJ = $(patsubst ./client/%.c, $(OBJ_DIR)/client/%.o, $(CLIENT_SRC))
CLIENT_TGT = client.exe

# server target
SERVER_SRC = $(wildcard ./server/*.c)
SERVER_INC = $(wildcard ./server/*.h)
SERVER_OBJ = $(patsubst ./server/%.c, $(OBJ_DIR)/server/%.o, $(SERVER_SRC))
SERVER_TGT = server.exe

all: $(OBJ_DIR) $(BIN_DIR) $(CLIENT_TGT) $(SERVER_TGT)
	mv $(CLIENT_TGT) $(BIN_DIR)
	mv $(SERVER_TGT) $(BIN_DIR)
	
print:
	@echo "Common files :"
	@echo $(COMMON_SRC) 
	@echo $(COMMON_INC)
	@echo $(COMMON_OBJ)
	@echo "-----------"
	@echo "Client files : "
	@echo $(CLIENT_SRC) 
	@echo $(CLIENT_INC)
	@echo $(CLIENT_OBJ)
	@echo "-----------"
	@echo "Server files : "
	@echo $(SERVER_SRC) 
	@echo $(SERVER_INC)
	@echo $(SERVER_OBJ)
	@echo "-----------"

# always execute this target
$(OBJ_DIR): FORCE
	mkdir -p $@
	mkdir -p $@/client
	mkdir -p $@/common
	mkdir -p $@/server

$(BIN_DIR):
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
	rm -f $(BIN_DIR)/$(CLIENT_TGT) $(BIN_DIR)/$(SERVER_TGT) 
	rm -rf $(OBJ_DIR)/*

.PHONY: all clean

FORCE: ;