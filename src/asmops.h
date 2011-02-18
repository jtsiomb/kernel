#ifndef ASMOPS_H_
#define ASMOPS_H_

#define enable_intr() asm volatile("sti")
#define disable_intr() asm volatile("cli")
#define halt_cpu() asm volatile("hlt")

#define inb(dest, port) asm volatile( \
	"inb %1, %0\n\t" \
	: "=a" ((unsigned char)(dest)) \
	: "dN" ((unsigned short)(port)))

#define inw(dest, port) asm volatile( \
	"inw %1, %0\n\t" \
	: "=a" ((unsigned short)(dest)) \
	: "dN" ((unsigned short)(port)))

#define inl(dest, port) asm volatile( \
	"inl %1, %0\n\t" \
	: "=a" ((unsigned long)(dest)) \
	: "dN" ((unsigned short)(port)))

#define outb(src, port) asm volatile( \
	"outb %0, %1\n\t" \
	:: "a" ((unsigned char)(src)), "dN" ((unsigned short)(port)))

#define outw(src, port) asm volatile( \
	"outw %0, %1\n\t" \
	:: "a" ((unsigned short)(src)), "dN" ((unsigned short)(port)))

#define outl(src, port) asm volatile( \
	"outl %0, %1\n\t" \
	:: "a" ((unsigned long)(src)), "dN" ((unsigned short)(port)))

#endif	/* ASMOPS_H_ */
