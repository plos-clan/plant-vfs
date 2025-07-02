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
lib: $(OBJS)
	ar rv libvfs.a $(OBJS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build libvfs.a