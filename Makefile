# collect all of our C and assembly source files
csrc = $(wildcard src/*.c) $(wildcard src/klibc/*.c)
asmsrc = $(wildcard src/*.S) $(wildcard src/klibc/*.S)
dep = $(asmsrc:.S=.d) $(csrc:.c=.d)

# each source file will generate one object file
obj = $(asmsrc:.S=.o) $(csrc:.c=.o)

CC = gcc

# -nostdinc instructs the compiler to ignore standard include directories
# -m32 instructs the compiler to produce 32bit code (in case we have a 64bit compiler)
CFLAGS = -m32 -Wall -g -nostdinc -fno-builtin -Isrc -Isrc/klibc
ASFLAGS = -m32 -g -nostdinc -fno-builtin -Isrc -Isrc/klibc

bin = kernel.elf

# default target: make an ELF binary by linking the object files
# we need to specify where to assume the text section (code) is going
# in memory, as well as the kernel entry point (kentry).
$(bin): $(obj)
	ld -melf_i386 -o $@ -Ttext 0x100000 -e kentry $(obj)

-include $(dep)

%.d: %.c
	@$(CPP) $(CFLAGS) -MM -MT $(@:.d=.o) $< >$@

%.d: %.S
	@$(CPP) $(ASFLAGS) -MM -MT $(@:.d=.o) $< >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
