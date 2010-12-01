# collect all of our C and assembly source files
csrc = $(wildcard src/*.c) $(wildcard src/klibc/*.c)
asmsrc = $(wildcard src/*.S) $(wildcard src/klibc/*.S)

# each source file will generate one object file
obj = $(csrc:.c=.o) $(asmsrc:.S=.o)

CC = gcc

# -nostdinc instructs the compiler to ignore standard include directories
# -m32 instructs the compiler to produce 32bit code (in case we have a 64bit compiler)
CFLAGS = -m32 -Wall -g -nostdinc -fno-builtin -Isrc -Isrc/klibc
ASFLAGS = -m32 -g -nostdinc -fno-builtin -Isrc -Isrc/klibc

bin = kernel.elf

# default target: make an ELF binary by linking the object files
# we need to specify where to assume the text segment (code) is going
# in memory, as well as the kernel entry point (kstart).
$(bin): $(obj)
	ld -melf_i386 -o $@ -Ttext 0x200000 -e _start $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)
