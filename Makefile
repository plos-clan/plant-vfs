CC ?= clang
CFLAGS := -I./include -std=gnu2x -Wno-pedantic -Wno-unused-function
DEBUG_CFLAGS := $(CFLAGS) -g -Og $(USER_CFLAGS)
RELEASE_CFLAGS := $(CFLAGS) -O3 -DNDEBUG $(USER_CFLAGS)
# RELEASE_CFLAGS += -m64 -nostdlib -fPIC -fno-builtin -fno-stack-protector
# RELEASE_CFLAGS += -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone

SRCS := vfs.c
OBJS := $(SRCS:%.c=build/%.o)

.PHONY: lib test valgrind clean

lib: CFLAGS := $(RELEASE_CFLAGS)
lib: $(OBJS)
	@echo '#define ALL_IMPLEMENTATION' | \
	cat - <(echo '#include <data-structure.h>') | \
	$(CC) $(CFLAGS) -x c -c - -o build/data_structure.o
	ar rv build/libvfs.a $(OBJS) build/data_structure.o

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: CFLAGS := $(DEBUG_CFLAGS)
test: lib
	$(CC) $(CFLAGS) -o build/memfs tests/memfs.c -Lbuild -lvfs
	build/memfs
valgrind: CFLAGS := $(DEBUG_CFLAGS)
valgrind: lib
	$(CC) $(CFLAGS) -o build/memfs tests/memfs.c -Lbuild -lvfs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes build/memfs

clean:
	rm -rf build
