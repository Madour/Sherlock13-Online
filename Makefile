CC=gcc
CCFLAGS= -Wall -Wpedentic
LIBFLAGS=

OBJ_DIR = ./obj

CLIENT_SRC = $(wildcard client/*.c)
CLIENT_INC = $(CLIENT_SRC:.c=.h)
CLIENT_OBJ = $(patsubst client/%.c, $(OBJ_DIR)/client/%.o, $(CLIENT_SRC))
CLIENT_TGT = client.exe



all: $(OBJ_DIR) $(CLIENT_TGT)
	
print:
	@echo $(CLIENT_SRC) $(CLIENT_INC)
	@echo $(CLIENT_OBJ)
	@echo $(OBJ_DIR)/client/%.o:
	@echo $(SRC_DIR)/client/%.c $(SRC_DIR)/client/%.h
	
$(OBJ_DIR):
	mkdir -p $@
	mkdir -p $@/client

$(CLIENT_TGT): $(CLIENT_OBJ)
	$(CC) $(LIBFLAGS) -o $@ $^ 

$(OBJ_DIR)/client/%.o: $(SRC_DIR)/client/%.c $(SRC_DIR)/client/%.h
	$(CC) $(CCFLAGS) -o $@ -c $<


.depend:
	$(CC) -MM $(CLIENT_SRC) > $@

-include .depends

clean:
	rm $(CLIENT_TGT) $(OBJ_DIR)/*

.PHONY: all clean
