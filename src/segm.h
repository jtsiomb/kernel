#ifndef SEGM_H_
#define SEGM_H_

#define SEGM_KCODE	1
#define SEGM_KDATA	2

typedef struct {
	uint16_t d[4];
} desc_t;

void init_segm(void);

uint16_t selector(int idx, int rpl);

/* these functions are implemented in segm-asm.S */
void setup_selectors(uint16_t code, uint16_t data);
void set_gdt(uint32_t addr, uint16_t limit);



#endif	/* SEGM_H_ */
