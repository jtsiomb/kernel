#include <assert.h>
#include "mutex.h"
#include "sched.h"
#include "intr.h"

void mutex_lock(mutex_t *m)
{
	int istate = get_intr_state();
	disable_intr();

	/* sleep while the mutex is held */
	while(*m > 0) {
		wait(m);
	}
	/* then grab it... */
	(*m)++;

	set_intr_state(istate);
}

void mutex_unlock(mutex_t *m)
{
	int istate = get_intr_state();
	disable_intr();

	assert(*m);
	/* release the mutex and wakeup everyone waiting on it */
	(*m)--;
	wakeup(m);

	set_intr_state(istate);
}

int mutex_trylock(mutex_t *m)
{
	int res = -1, istate = get_intr_state();
	disable_intr();

	if(*m == 0) {
		(*m)++;
		res = 0;
	}
	set_intr_state(istate);
	return res;
}
