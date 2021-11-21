CFLAGS += -std=c99 -Wall
LDFLAGS += -ledit -lm -Lmpc
SRCDIR += src
LIBDIR := mpc
SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(shell find $(SRCDIR) -name '*.o')
BIN = lisp
CLANG_FORMAT = clang-format-11

lib:
	make -C $(LIBDIR) build
	make -C $(LIBDIR) build/libmpc.so

build: lib $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

fmt:
	$(CLANG_FORMAT) -i $(SRC)

clean:
	@rm $(OBJ) $(BIN)

clean-all:
	make -C $(LIBDIR) clean

all: fmt build
