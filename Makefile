CFLAGS += -g -std=c99 -Wall -I/usr/local/share/mpc
LDFLAGS += -ledit -lmpc -L/usr/local/lib/
SRCDIR += src
TESTDIR += lsp
SRC := $(shell find $(SRCDIR) -name '*.c')
HDR := $(shell find $(SRCDIR) -name '*.h')
OBJ := $(shell find $(SRCDIR) -name '*.o')
LSP_TEST := $(shell find $(TESTDIR) -name 'test_*.lsp')
LSP_LIB:= $(shell find $(TESTDIR) \( -name '*.lsp' ! -name 'test_*.lsp' \))
BIN = lisp
CLANG_FORMAT = clang-format-11

all: build tags

lib:
	make -C mpc build/libmpc.so
	sudo make -C install

build: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

fmt:
	$(CLANG_FORMAT) -i $(SRC) $(HDR)

tags:
	ctags -R .
clean:
	@rm $(OBJ) $(BIN) tags

clean-lib:
	make -C mpc clean

clean-all: clean clean-lib

test: all $(TEST)
	./$(BIN) $(LSP_LIB) $(LSP_TEST)
