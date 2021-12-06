CFLAGS += -g -std=c99 -Wall -I/usr/local/share/mpc
LDFLAGS += -ledit -lmpc -L/usr/local/lib/
SRCDIR += src
TESTDIR += lsp
SRC := $(shell find $(SRCDIR) -name '*.c')
HDR := $(shell find $(SRCDIR) -name '*.h')
OBJ := $(shell find $(SRCDIR) -name '*.o')
CODE := $(SRC) $(HDR)
LSP_TEST := $(shell find $(TESTDIR) -name 'test_*.lsp')
LSP_LIB:= $(shell find $(TESTDIR) \( -name '*.lsp' ! -name 'test_*.lsp' \))
BIN = lisp
CLANG_FORMAT = clang-format-11
TEST = ./$(BIN) $(LSP_LIB) $(LSP_TEST)
PROFRAW = tests.profraw
PROFDATA = tests.profdata
COVERAGE = llvm-cov report $(TEST) -instr-profile=$(PROFDATA) $(CODE)

.PHONY: all
all: build tags

.PHONY: lib
lib:
	make -C mpc build/libmpc.so
	sudo make -C install

build: $(CODE)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

.PHONY: fmt
fmt:
	$(CLANG_FORMAT) -i $(SRC) $(HDR)

tags: $(CODE)
	ctags -R .

.PHONY: clean
clean:
	@rm $(OBJ) $(BIN) tags $(PROFRAW) $(PROFDATA) default.profraw

.PHONY: clean-lib
clean-lib:
	make -C mpc clean

.PHONY: clean-all
clean-all: clean clean-lib


.PHONY: test-valgrind
test-asan: all
	clang $(CFLAGS) -fsanitize=address $(SRC) $(LDFLAGS) -o $(BIN)
	$(TEST)


.PHONY: test-valgrind
test-valgrind: all
	valgrind                                       \
		--leak-check=full                      \
		--track-origins=yes                    \
		--exit-on-first-error=yes              \
		$(TEST)

.PHONY: test-valgrind-dbg
test-valgrind-dbg: all
	valgrind                                       \
		--leak-check=full                      \
		--track-origins=yes                    \
		--vgdb-error=0                         \
		$(TEST)

.PHONY:run-coverage
run-coverage: $(CODE)
	clang                                                         \
		-fprofile-instr-generate                              \
		-fcoverage-mapping                                    \
		-Wno-gnu-zero-variadic-macro-arguments                \
		$(CFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

	LLVM_PROFILE_FILE="$(PROFRAW)" $(TEST)
	llvm-profdata merge -sparse $(PROFRAW) -o $(PROFDATA)
	llvm-cov show $(TEST) -instr-profile=$(PROFDATA)

.PHONY:coverage
coverage: $(CODE) run-coverage
	$(COVERAGE)

.PHONY:coverage-function-summary
coverage-function-summary: $(CODE) run-coverage
	$(COVERAGE) --show-functions

.PHONY: test
test: all
	$(TEST)
