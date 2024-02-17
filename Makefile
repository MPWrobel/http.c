CC=$(shell which clang)
CFLAGS=-g
# CFLAGS+=-fsanitize=address,undefined
SRC=$(wildcard src/*.c)
HEADERS=$(wildcard src/*.h)

.PHONY: all clean install

all: clean http

http: $(SRC) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(SRC) -o build/$@

clean:
	rm -rf build

run:
	ASAN_OPTIONS=detect_leaks=1 ./build/http 8080

install:
	cp build/http ~/.local/bin/
