# Compiler selection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CC = clang
else
    CC = gcc
endif

# Common flags
CFLAGS_COMMON = 
TARGET        = gex
SRC           = gex.c file_handling.c gex_helper_funcs.c keyb_man.c win_man.c rbtree.c
OBJ           = $(SRC:.c=.o)

# NCURSES flags
ifeq ($(UNAME_S),Darwin)
    # macOS: use system ncurses, panel
    NCURSES_FLAGS = -lncurses -lpanel
else
    # Linux: use Homebrew's ncurses
    NCURSES_PREFIX := $(shell brew --prefix ncurses)
    NCURSES_FLAGS = -I$(NCURSES_PREFIX)/include -L$(NCURSES_PREFIX)/lib -lncurses -lpanel
endif

.PHONY: all clean release tidy

# Default target
all: release

# Release build
release: CFLAGS = $(CFLAGS_COMMON)
release: $(TARGET)

# debug build
debug: CFLAGS = -Wall -Wextra -fsanitize=address -g -O1 -Wshadow  -Wcast-qual -Wpedantic
debug: $(TARGET)


tidy:
	xcrun clang-tidy $(SRC) \
		-checks='clang-diagnostic-*,clang-analyzer-*,misc-*,-misc-include-cleaner' \
		-- -Wall -Wextra

maxtidy:
	xcrun clang-tidy $(SRC) \
		-checks='clang-diagnostic-*,clang-analyzer-*,misc-*,-misc-include-cleaner' \
		-- -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wcast-qual -Wpedantic

# Build rules
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(NCURSES_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TARGET) $(OBJ)
