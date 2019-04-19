DIR_INC = .
DIR_SRC = .
DIR_OBJ = .
DIR_BIN = .

SRC = $(wildcard $(DIR_SRC)/*.c)
OBJ := $(patsubst %.c,$(DIR_OBJ)/%.o,$(notdir $(SRC)))

TARGET = mongoose.a
BIN_TARGET = $(DIR_BIN)/$(TARGET)

CC = gcc

AR = ar

INCLUDE := -I./

LDFLAGS := -DMG_ENABLE_HTTP_STREAMING_MULTIPART -DMG_ENABLE_HTTP_URL_REWRITES -DMG_ENABLE_SSL 

LIBS :=	-pthread -lssl -lcrypto

CFLAGS := $(INCLUDE) $(LDFLAGS) $(LIBS)

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c
	$(CC) -c $< -o $@  $(CFLAGS) 

$(BIN_TARGET) : $(OBJ)
	$(AR) -crv $@ $(OBJ)

clean:
	rm $(wildcard $(DIR_OBJ)/*.o) $(BIN_TARGET)

