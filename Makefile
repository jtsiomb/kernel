ifneq ($(shell uname -m), i386)
	# -m32 instructs the compiler to produce 32bit code
	ccemu = -m32

	ifeq ($(shell uname -s), FreeBSD)
		ldemu = -m elf_i386_fbsd
	else
		ldemu = -m elf_i386
	endif
endif

# collect all of our C and assembly source files
csrc = $(wildcard src/boot/*.c) $(wildcard src/*.c) $(wildcard src/klibc/*.c)
asmsrc = $(wildcard src/boot/*.S) $(wildcard src/*.S) $(wildcard src/klibc/*.S)
dep = $(asmsrc:.S=.d) $(csrc:.c=.d)

# each source file will generate one object file
obj = $(asmsrc:.S=.o) $(csrc:.c=.o)

CC = gcc

inc = -Isrc -Isrc/klibc -Isrc/boot -Iinclude

# -nostdinc instructs the compiler to ignore standard include directories
CFLAGS = $(ccemu) -Wall -g -nostdinc -fno-builtin $(inc) -DKERNEL
ASFLAGS = $(ccemu) -g -nostdinc -fno-builtin $(inc)

bin = kernel.elf

# default target: make an ELF binary by linking the object files
# we need to specify where to assume the text section (code) is going
# in memory, as well as the kernel entry point (kentry).
$(bin): $(obj)
	ld $(ldemu) -o $@ -Ttext 0x100000 -e kentry $(obj) -Map link.map

%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

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
