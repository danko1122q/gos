#include "gos.h"
#include <time.h>
#include <sys/time.h>
#include <limits.h>

static struct tm tm_static;
static const char *const wday_short[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const wday_long[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const mon_short[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char *const mon_long[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int month_days(int y, int m) {
    static const unsigned char days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    return days[m] + (m == 1 && leap(y));
}
static int year_day(int y, int m, int d) {
    int n = d - 1; for (int i = 0; i < m; i++) n += month_days(y, i); return n;
}
static void from_epoch(time_t value, struct tm *out) {
    DateTime dt;
    epoch_to_dt(value, &dt);
    out->tm_sec = dt.sec; out->tm_min = dt.min; out->tm_hour = dt.hour;
    out->tm_mday = dt.day; out->tm_mon = dt.mon - 1; out->tm_year = dt.year - 1900;
    out->tm_wday = dt.wday; out->tm_yday = year_day(dt.year, dt.mon - 1, dt.day); out->tm_isdst = 0;
}
time_t time(time_t *timer) { time_t t = gos_time(); if (timer) *timer = t; return t; }
clock_t clock(void) {
    int pid = gos_getpid();
    GProcInfo pi;
    for (int i = 0; i < GOS_MAXTASK; i++) {
        if (gos_procinfo(i, &pi) == 0 && pi.pid == pid) {
            GSysInfo si; u32 hz = 100;
            if (gos_sysinfo(&si) == 0 && si.hz) hz = si.hz;
            u32 whole = pi.cpu / hz, part = pi.cpu % hz;
            if (whole > 0x7fffffffu / CLOCKS_PER_SEC) return (clock_t)-1;
            return (clock_t)(whole * CLOCKS_PER_SEC + (part * CLOCKS_PER_SEC) / hz);
        }
    }
    errno = ESRCH;
    return (clock_t)-1;
}
struct tm *gmtime_r(const time_t *t, struct tm *out) { if (!t || !out) { errno = EINVAL; return NULL; } from_epoch(*t, out); return out; }
struct tm *localtime_r(const time_t *t, struct tm *out) { return gmtime_r(t, out); }
struct tm *gmtime(const time_t *t) { return gmtime_r(t, &tm_static); }
struct tm *localtime(const time_t *t) { return localtime_r(t, &tm_static); }

time_t mktime(struct tm *t) {
    if (!t) { errno = EINVAL; return (time_t)-1; }
    int y = t->tm_year + 1900, m = t->tm_mon;
    while (m < 0) { m += 12; y--; }
    while (m >= 12) { m -= 12; y++; }
    if (y < 1970 || y > 2106) { errno = EOVERFLOW; return (time_t)-1; }
    int sec = t->tm_sec, min = t->tm_min, hour = t->tm_hour, day = t->tm_mday;
    while (sec < 0) { sec += 60; min--; }
    while (sec >= 60) { sec -= 60; min++; }
    while (min < 0) { min += 60; hour--; }
    while (min >= 60) { min -= 60; hour++; }
    while (hour < 0) { hour += 24; day--; }
    while (hour >= 24) { hour -= 24; day++; }
    while (day < 1) { if (--m < 0) { m = 11; y--; } day += month_days(y, m); }
    while (day > month_days(y, m)) { day -= month_days(y, m); if (++m >= 12) { m = 0; y++; } }
    if (y < 1970 || y > 2106) { errno = EOVERFLOW; return (time_t)-1; }
    u32 days = 0;
    for (int yy = 1970; yy < y; yy++) days += leap(yy) ? 366u : 365u;
    days += (u32)year_day(y, m, day);
    u32 daytime = (u32)hour * 3600u + (u32)min * 60u + (u32)sec;
    if (days > 49710u || (days == 49710u && daytime > 0xffffffffu - days * 86400u)) { errno = EOVERFLOW; return (time_t)-1; }
    u32 result = days * 86400u + daytime;
    from_epoch(result, t);
    return result;
}
int clock_gettime(int id, struct timespec *tp) {
    if (!tp) { errno = EFAULT; return -1; }
    if (id == CLOCK_REALTIME) { tp->tv_sec = gos_time(); tp->tv_nsec = 0; return 0; }
    if (id == CLOCK_MONOTONIC) {
        GSysInfo si; u32 hz = 100, ticks = gos_ticks();
        if (gos_sysinfo(&si) == 0 && si.hz) hz = si.hz;
        u32 rem = ticks % hz;
        u32 q = 1000000000u / hz, r = 1000000000u % hz;
        tp->tv_sec = ticks / hz;
        tp->tv_nsec = (s32)(rem * q + (rem * r) / hz);
        return 0;
    }
    errno = EINVAL; return -1;
}
int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz; if (!tv) { errno = EFAULT; return -1; }
    tv->tv_sec = gos_time(); tv->tv_usec = 0; return 0;
}

static int append(char *s, size_t max, size_t *n, const char *v) {
    while (*v) { if (*n + 1 >= max) return -1; s[(*n)++] = *v++; }
    return 0;
}
static int append_num(char *s, size_t max, size_t *n, int v, int width) {
    char b[16]; int k = 0;
    if (v < 0) v = 0;
    do { b[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < (int)sizeof b);
    while (k < width) b[k++] = '0';
    while (k) { if (*n + 1 >= max) return -1; s[(*n)++] = b[--k]; }
    return 0;
}
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *t) {
    size_t n = 0;
    if (!s || !fmt || !t || !max) return 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { if (n + 1 >= max) return 0; s[n++] = *fmt; continue; }
        fmt++; int r = 0;
        switch (*fmt) {
        case '%': r = append(s, max, &n, "%"); break;
        case 'a': r = append(s, max, &n, wday_short[(unsigned)t->tm_wday % 7]); break;
        case 'A': r = append(s, max, &n, wday_long[(unsigned)t->tm_wday % 7]); break;
        case 'b': case 'h': r = append(s, max, &n, mon_short[(unsigned)t->tm_mon % 12]); break;
        case 'B': r = append(s, max, &n, mon_long[(unsigned)t->tm_mon % 12]); break;
        case 'c': r = append(s, max, &n, wday_short[(unsigned)t->tm_wday % 7]); if (!r) r = append(s,max,&n," "); if (!r) r = append(s,max,&n,mon_short[(unsigned)t->tm_mon % 12]); if (!r) r = append(s,max,&n," "); if (!r) r = append_num(s,max,&n,t->tm_mday,2); if (!r) r = append(s,max,&n," "); if (!r) r = append_num(s,max,&n,t->tm_hour,2); if (!r) r = append(s,max,&n,":"); if (!r) r = append_num(s,max,&n,t->tm_min,2); if (!r) r = append(s,max,&n,":"); if (!r) r = append_num(s,max,&n,t->tm_sec,2); if (!r) r = append(s,max,&n," "); if (!r) r = append_num(s,max,&n,t->tm_year+1900,4); break;
        case 'd': r = append_num(s,max,&n,t->tm_mday,2); break;
        case 'e': r = append_num(s,max,&n,t->tm_mday,2); if (!r && n >= 2 && s[n-2] == '0') s[n-2] = ' '; break;
        case 'H': r = append_num(s,max,&n,t->tm_hour,2); break;
        case 'I': r = append_num(s,max,&n,(t->tm_hour % 12) ? t->tm_hour % 12 : 12,2); break;
        case 'j': r = append_num(s,max,&n,t->tm_yday+1,3); break;
        case 'm': r = append_num(s,max,&n,t->tm_mon+1,2); break;
        case 'M': r = append_num(s,max,&n,t->tm_min,2); break;
        case 'p': r = append(s,max,&n,t->tm_hour < 12 ? "AM" : "PM"); break;
        case 'S': r = append_num(s,max,&n,t->tm_sec,2); break;
        case 'w': r = append_num(s,max,&n,t->tm_wday,1); break;
        case 'u': r = append_num(s,max,&n,t->tm_wday ? t->tm_wday : 7,1); break;
        case 'y': r = append_num(s,max,&n,(t->tm_year+1900)%100,2); break;
        case 'Y': r = append_num(s,max,&n,t->tm_year+1900,4); break;
        case 'x': r = append_num(s,max,&n,t->tm_mon+1,2); if (!r) r = append(s,max,&n,"/"); if (!r) r = append_num(s,max,&n,t->tm_mday,2); if (!r) r = append(s,max,&n,"/"); if (!r) r = append_num(s,max,&n,(t->tm_year+1900)%100,2); break;
        case 'X': r = append_num(s,max,&n,t->tm_hour,2); if (!r) r = append(s,max,&n,":"); if (!r) r = append_num(s,max,&n,t->tm_min,2); if (!r) r = append(s,max,&n,":"); if (!r) r = append_num(s,max,&n,t->tm_sec,2); break;
        case 'Z': r = append(s,max,&n,"UTC"); break;
        case 'z': r = append(s,max,&n,"+0000"); break;
        default: if (n + 2 >= max) return 0; s[n++] = '%'; s[n++] = *fmt; break;
        }
        if (r < 0) return 0;
    }
    s[n] = 0; return n;
}

int nanosleep(const struct timespec *request, struct timespec *remaining) {
    if (!request || (s32)request->tv_sec < 0 || request->tv_nsec < 0 || request->tv_nsec >= 1000000000) {
        errno = EINVAL; return -1;
    }
    if (remaining) { remaining->tv_sec = 0; remaining->tv_nsec = 0; }
    u32 seconds = request->tv_sec;
    if (seconds > 0x7fffffffu / 1000u) { errno = EINVAL; return -1; }
    u32 ms = seconds * 1000u + ((u32)request->tv_nsec + 999999u) / 1000000u;
    if (ms > 0x7fffffffu) { errno = EINVAL; return -1; }
    gos_sleep_ms((int)ms);
    return 0;
}
int timespec_get(struct timespec *ts, int base) {
    if (!ts || base != TIME_UTC) return 0;
    ts->tv_sec = gos_time(); ts->tv_nsec = 0;
    return TIME_UTC;
}
