#ifndef SEGM_H_
#define SEGM_H_

#define SEGM_KCODE	1
#define SEGM_KDATA	2

void init_segm(void);

uint16_t selector(int idx, int rpl);


#endif	/* SEGM_H_ */
