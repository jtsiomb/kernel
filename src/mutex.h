#ifndef MUTEX_H_
#define MUTEX_H_

typedef unsigned int mutex_t;

void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

int mutex_trylock(mutex_t *m);

#endif	/* MUTEX_H_ */
