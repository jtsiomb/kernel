#ifndef _TIME_H_
#define _TIME_H_

typedef long time_t;

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

time_t time(time_t *tp);
char *asctime(struct tm *tm);
char *asctime_r(struct tm *tm, char *buf);

time_t mktime(struct tm *tm);
struct tm *gmtime(time_t *tp);
struct tm *gmtime_r(time_t *tp, struct tm *tm);

/* non-standard helpers */
int day_of_year(int year, int mon, int day);


#endif	/* _TIME_H_ */
