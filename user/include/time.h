#ifndef GOS_TIME_H
#define GOS_TIME_H
#include <sys/types.h>
#define CLOCKS_PER_SEC 1000
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define TIME_UTC 1
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};
struct timespec { time_t tv_sec; s32 tv_nsec; };
time_t time(time_t *timer);
clock_t clock(void);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
struct tm *gmtime_r(const time_t *timer, struct tm *result);
struct tm *localtime_r(const time_t *timer, struct tm *result);
time_t mktime(struct tm *tm);
int clock_gettime(int clock_id, struct timespec *tp);
int nanosleep(const struct timespec *request, struct timespec *remaining);
int timespec_get(struct timespec *ts, int base);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
#endif
