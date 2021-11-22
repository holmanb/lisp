CFLAGS += -g -std=c99 -Wall -I/usr/local/share/mpc
LDFLAGS += -ledit -lmpc -L/usr/local/lib/
SRCDIR += src
SRC := $(shell find $(SRCDIR) -name '*.c')
OBJ := $(shell find $(SRCDIR) -name '*.o')
BIN = lisp
CLANG_FORMAT = clang-format-11

lib:
	make -C mpc build/libmpc.so
	mv mpc/build/libmpc.so mpc

build: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

fmt:
	$(CLANG_FORMAT) -i $(SRC)

clean:
	@rm $(OBJ) $(BIN)

clean-lib:
	make -C mpc clean

clean-all: clean clean-lib

all: build

run:
	LD_LIBRARY_PATH+=/usr/local/lib/ ./$(BIN)
