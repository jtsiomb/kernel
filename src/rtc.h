#ifndef _RTC_H_
#define _RTC_H_

#include <time.h>

/* the time read from rtc during init */
time_t start_time;

void init_rtc(void);

#endif	/* _RTC_H_ */
