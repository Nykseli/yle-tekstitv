
# all:
# 	gcc -Ithird_party/tidy-html5/include src/main.c third_party/tidy-html5/build/cmake/libtidys.a
#
# # https://stackoverflow.com/a/15065359 < shared linking
# # shared linking only works on dev
# shared:
# 	gcc -Ithird_party/tidy-html5/include -Lthird_party/tidy-html5/build/cmake/ -Wl,-Rthird_party/tidy-html5/build/cmake/ src/main.c -ltidy
#
# clean:
# 	rm a.out
#
# .PHONY: all clean


# Makefile for building a single configuration of the C interpreter. It expects
# variables to be passed in for:
#
# MODE         "debug" or "release".
# NAME         Name of the output executable (and object file directory).
# SOURCE_DIR   Directory where source files and headers are found.

NAME=tekstitv
# CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS := -Wall -Wextra -Wno-unused-parameter
SOURCE_DIR = src

# Mode configuration.
ifeq ($(MODE),debug)
	CFLAGS += -O0 -DDEBUG -g
	BUILD_DIR := build/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := build/release
endif

# static or so build
ifeq ($(LINK),shared)
	LINK_PATHS := -Lthird_party/tidy-html5/build/cmake/ \
					-Wl,-Rthird_party/tidy-html5/build/cmake/ \
					-Lthird_party/ncurses/lib \
					-Wl,-Rthird_party/ncurses/lib
	LINKS := -ltidy -lncurses -lcurl
else
	LINKS := third_party/tidy-html5/build/cmake/libtidys.a \
				third_party/ncurses/lib/libncursesw.a
endif
# Files.
INCLUDES := -Ithird_party/tidy-html5/include -Ithird_party/ncurses/include
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/$(NAME)/, $(notdir $(SOURCES:.c=.o)))

# Targets ---------------------------------------------------------------------

# Link the interpreter.
build/$(NAME): $(OBJECTS)
	@ printf "%8s %-40s %s\n" $(CC) $@ "$(CFLAGS)"
	@ mkdir -p build
	@ $(CC) $(INCLUDES) $(LINK_PATHS) $(CFLAGS) $^ -o $@ $(LINKS)

# Compile object files.
$(BUILD_DIR)/$(NAME)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%8s %-40s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/$(NAME)
	@ $(CC) $(INCLUDES) -c $(CFLAGS) -o $@ $<

.PHONY: default
