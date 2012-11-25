#LUAJIT = /usr/local/openresty/luajit
PREFIX=/usr/local
#LIBDIR ?= ${LUAJIT}/lib
#LUAINC ?= ${LUAJIT}/include
LIBDIR ?= ${PREFIX}/lib
LUAINC ?= ${PREFIX}/include
LUALIB ?= lua-5.1
#LUALIB ?= luajit
#LIBDIR = /usr/local/openresty/lualib

LIB = amf_codec.so

SRC = src/amf_codec.c src/amf_buf.c src/amf_cursor.c src/amf_remoting.c src/lua-amf-codec.c

OBJS = ${SRC:.c=.o}

CFLAGS  += -g -fPIC -std=c99 -pedantic -Wall -Wextra -Wshadow -Wformat -Wundef -Wwrite-strings -I${LUAINC}
LDFLAGS += -undefined dynamic_lookup

.PHONY: all

all: ${LIB}

${LIB}: ${OBJS}
	cc -shared -o $@ ${OBJS} ${CFLAGS} ${LDFLAGS}

clean:
	rm -f ${LIB} ${OBJS}

install: all
	install -d ${LIBDIR}
	install ${LIB} ${LIBDIR}

LUA_TESTS=$(wildcard tests/*.lua)

test: all
	busted test/encode_test.lua
	busted test/decode_test.lua
