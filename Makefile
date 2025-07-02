CC ?= clang
CFLAGS := -I./include -std=gnu2x -Wno-pedantic -Wno-unused-function
DEBUG_CFLAGS := $(CFLAGS) -g -Og $(USER_CFLAGS)
RELEASE_CFLAGS := $(CFLAGS) -O3 -DNDEBUG $(USER_CFLAGS)
# RELEASE_CFLAGS += -m64 -nostdlib -fPIC -fno-builtin -fno-stack-protector
# RELEASE_CFLAGS += -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone

SRCS := vfs.c
OBJS := $(SRCS:%.c=build/%.o)

.PHONY: lib test clean

lib: CFLAGS := $(RELEASE_CFLAGS)
lib: $(OBJS) libds.a
	ar rv libvfs.a $(OBJS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
libds.a: data_structure/data_structure.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o build/data_structure.o
	ar rcs libds.a build/data_structure.o
clean:
	rm -rf build libvfs.a libds.a