# Makefile for building the Lua inotify shared library
# Note: this is not needed if you use LuaRocks to install the module

CC      = cc
CFLAGS  = -std=c99 -O2 -Wall -Wextra -Werror -fPIC
LDFLAGS = -shared -llua

SRC = linotify.c
LIB = inotify.so

all: $(LIB)

$(LIB): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -rf $(LIB)

.PHONY: all clean
