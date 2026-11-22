#ifndef LIB_KLOG_H
#define LIB_KLOG_H
#define KLOG_SIZE 16384
void klog_write(const char *s);
int  klog_read(char *buf, int max);
int  klog(const char *fmt, ...);
#endif
