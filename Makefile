CFLAGS += -std=c99 -Wall
LDFLAGS += -ledit
SRC := $(shell find -name '*.c')
OBJ := $(shell find -name '*.o')
BIN = lisp
CLANG_FORMAT = clang-format-11

build: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

fmt:
	$(CLANG_FORMAT) -i $(SRC)

clean:
	@rm $(OBJ) $(BIN)

all: fmt build
