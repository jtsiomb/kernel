#ifndef _TIMER_H_
#define _TIMER_H_

typedef void (*timer_func_t)(void*);

unsigned long nticks;

void init_timer(void);

int start_timer(unsigned long msec, timer_func_t cbfunc, void *cbarg);

#endif	/* _TIMER_H_ */
