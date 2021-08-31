#ifndef _THREADPOOL_H
#define _THREADPOOL_H

int ahttpd_threadpool_create(int threads_count_min, int threads_count_max, int tasks_count_max);

int ahttpd_threadpool_add(void (*func)(void *arg), void *arg);

int ahttpd_threadpool_destroy(void);

#endif
