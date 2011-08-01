#ifndef _CONFIG_H_
#define _CONFIG_H_

/* frequency of generated timer ticks in hertz */
#define TICK_FREQ_HZ		250

#define TIMESLICE			100
#define TIMESLICE_TICKS		(TIMESLICE * TICK_FREQ_HZ / 1000)

#endif	/* _CONFIG_H_ */
