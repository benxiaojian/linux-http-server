DIR_INC = ./inc
DIR_SRC = ./src
DIR_OBJ = ./obj
DIR_BIN = ./bin

SRC = $(wildcard $(DIR_SRC)/*.cpp)
OBJ := $(patsubst %.cpp,$(DIR_OBJ)/%.o,$(notdir $(SRC)))

TARGET = http_server
BIN_TARGET = $(DIR_BIN)/$(TARGET)

CC = g++

INCLUDE := -I./inc -I./common

LDFLAGS := ./common/mongoose.a

LIBS :=

CFLAGS := -std=c++11 $(INCLUDE) $(LIBS) -g

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.cpp
	$(CC) -c $< -o $@  $(CFLAGS) 

$(BIN_TARGET) : $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	rm $(wildcard $(DIR_OBJ)/*.o) $(BIN_TARGET)

