# Compiler selection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CC = clang
else
    CC = gcc
endif

# Common flags
CFLAGS_COMMON = 
TARGET        = gls
SRC           = gls.c options.c display.c
OBJ           = $(SRC:.c=.o)

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
	$(CC) $(CFLAGS) -o $@ $(OBJ) 

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TARGET) $(OBJ)
