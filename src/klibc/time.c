#include <stdio.h>
#include "time.h"

#define MINSEC		60
#define HOURSEC		(60 * MINSEC)
#define DAYSEC		(24 * HOURSEC)

static int is_leap_year(int yr);


static char *wday[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char *mon[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


char *asctime(struct tm *tm)
{
	static char buf[64];

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
		days += is_leap_year(year++) ? 366 : 365;
	}

	return (time_t)days * DAYSEC + tm->tm_hour * HOURSEC +
		tm->tm_min * MINSEC + tm->tm_sec;
}

int day_of_year(int year, int mon, int day)
{
	static int commdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	static int leapdays[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int *mdays, i, yday;

	mdays = is_leap_year(year) ? leapdays : commdays;

	yday = day;
	for(i=0; i<mon; i++) {
		yday += mdays[i];
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
