#ifndef SEGM_H_
#define SEGM_H_

#define SEGM_KCODE	1
#define SEGM_KDATA	2

typedef struct {
	uint16_t d[4];
} desc_t;

void init_segm(void);

uint16_t selector(int idx, int rpl);


#endif	/* SEGM_H_ */
