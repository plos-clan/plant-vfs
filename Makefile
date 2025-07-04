CC ?= clang
CFLAGS := -I./include -std=gnu2x -Wno-pedantic -Wno-unused-function
DEBUG_CFLAGS := $(CFLAGS) -g -Og $(USER_CFLAGS)
RELEASE_CFLAGS := $(CFLAGS) -O3 -DNDEBUG $(USER_CFLAGS)
# RELEASE_CFLAGS += -m64 -nostdlib -fPIC -fno-builtin -fno-stack-protector
# RELEASE_CFLAGS += -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone

SRCS := vfs.c
OBJS := $(SRCS:%.c=build/%.o)

.PHONY: lib test clean example valgrind valgrind-full

lib: CFLAGS := $(RELEASE_CFLAGS)
lib: $(OBJS)
	@mkdir -p build
	@echo '#define ALL_IMPLEMENTATION' > data_structure.c
	@echo '#include <data-structure.h>' >> data_structure.c
	$(CC) $(CFLAGS) -c data_structure.c -o build/data_structure.o
	ar rv libvfs.a $(OBJS) build/data_structure.o

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


test: CFLAGS := $(DEBUG_CFLAGS)
test: lib
	$(CC) $(CFLAGS) -o memfs memfs.c -L. -lvfs
	./memfs
valgrind: CFLAGS := $(DEBUG_CFLAGS)
valgrind: lib
	$(CC) $(CFLAGS) -o memfs memfs.c -L. -lvfs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./memfs

clean:
	rm -rf build libvfs.a memfs data_structure.c
