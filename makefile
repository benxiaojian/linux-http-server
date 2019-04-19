DIR_INC = ./inc
DIR_SRC = ./src
DIR_OBJ = ./obj
DIR_BIN = ./bin

# include ./common/makefile

SRC = $(wildcard $(DIR_SRC)/*.cpp)
OBJ := $(patsubst %.cpp,$(DIR_OBJ)/%.o,$(notdir $(SRC)))

TARGET = http_server
BIN_TARGET = $(DIR_BIN)/$(TARGET)

CC = g++

INCLUDE := -I./inc -I./common

LDFLAGS := ./common/mongoose.a

LIBS :=	-lmysqlclient -lssl -lcrypto

CFLAGS := -std=c++11 $(INCLUDE) $(LIBS) -DMG_ENABLE_HTTP_STREAMING_MULTIPART -DMG_ENABLE_HTTP_URL_REWRITES -DMG_ENABLE_SSL -g

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.cpp
	$(CC) -c $< -o $@  $(CFLAGS)

$(BIN_TARGET) : $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) $(CFLAGS) 

clean:
	rm $(wildcard $(DIR_OBJ)/*.o) $(BIN_TARGET)

