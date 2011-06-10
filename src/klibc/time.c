#include <stdio.h>
#include "time.h"
#include "rtc.h"
#include "timer.h"
#include "config.h"

#define MINSEC		60
#define HOURSEC		(60 * MINSEC)
#define DAYSEC		(24 * HOURSEC)
#define YEARDAYS(x)	(is_leap_year(x) ? 366 : 365)

static int is_leap_year(int yr);

static int mdays[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static char *wday[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char *mon[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


time_t time(time_t *tp)
{
	time_t res = start_time + nticks / TICK_FREQ_HZ;

	if(tp) *tp = res;
	return res;
}

char *asctime(struct tm *tm)
{
	static char buf[64];
	return asctime_r(tm, buf);
}

char *asctime_r(struct tm *tm, char *buf)
{
	sprintf(buf, "%s %s %d %02d:%02d:%02d %d\n", wday[tm->tm_wday],
			mon[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min,
			tm->tm_sec, tm->tm_year + 1900);
	return buf;
}

time_t mktime(struct tm *tm)
{
	int i, num_years = tm->tm_year - 70;
	int year = 1970;
	int days = day_of_year(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday - 1);

	for(i=0; i<num_years; i++) {
		days += YEARDAYS(year++);
	}

	return (time_t)days * DAYSEC + tm->tm_hour * HOURSEC +
		tm->tm_min * MINSEC + tm->tm_sec;
}

struct tm *gmtime(time_t *tp)
{
	static struct tm tm;
	return gmtime_r(tp, &tm);
}

struct tm *gmtime_r(time_t *tp, struct tm *tm)
{
	int year, days, leap, yrdays;
	time_t t;

	year = 1970;
	days = *tp / DAYSEC;
	t = *tp % DAYSEC;

	tm->tm_wday = (days + 4) % 7;

	while(days >= (yrdays = YEARDAYS(year))) {
		days -= yrdays;
		year++;
	}
	tm->tm_year = year - 1900;

	leap = is_leap_year(year);
	tm->tm_mon = 0;
	while(days >= mdays[leap][tm->tm_mon]) {
		days -= mdays[leap][tm->tm_mon++];
	}

	tm->tm_mday = days + 1;

	tm->tm_hour = t / HOURSEC;
	t %= HOURSEC;
	tm->tm_min = t / MINSEC;
	tm->tm_sec = t % MINSEC;

	tm->tm_yday = day_of_year(year, tm->tm_mon, days);
	return tm;
}

int day_of_year(int year, int mon, int day)
{
	int i, yday, leap;

	leap = is_leap_year(year) ? 1 : 0;
	yday = day;

	for(i=0; i<mon; i++) {
		yday += mdays[leap][i];
	}
	return yday;
}

static int is_leap_year(int yr)
{
	/* exceptions first */
	if(yr % 400 == 0) {
		return 1;
	}
	if(yr % 100 == 0) {
		return 0;
	}
	/* standard case */
	return yr % 4 == 0;
}
