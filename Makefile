CC=clang

LUADIR=./3rd-party/lua-5.4.7/src/
LUALIB=$(LUADIR)/liblua.a

LFLAGS=-L/opt/homebrew/opt/libwebsockets/lib \
			 -L./3rd-party/lua-5.4.7/src

LIBS=-llua -lwebsockets -liconv

OUT=lua_test
TEST=./tests/test1.lua

INCLUDE=./include
SRC=./src

CFLAGS=-I$(LUADIR) \
			 -I$(INCLUDE) \
			 -I/opt/homebrew/opt/libwebsockets/include/ \
			 -I/opt/homebrew/opt/openssl@3/include

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard include/*.h) 

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OUT) | Makefile

$(OUT): $(OBJS) $(LUALIB)
	$(CC) $(OBJS) -o $@ $(LFLAGS) $(LIBS)


run: $(OUT)
	./$(OUT) $(TEST)
