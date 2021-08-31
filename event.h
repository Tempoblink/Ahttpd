#ifndef _KQUEUE_H
#define _KQUEUE_H
#include <sys/event.h>

void ahttpd_event_handler(int sockfd, int backlog, void (*func)(void *));
#endif
