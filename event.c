#include "event.h"
#include "http.h"
#include "log.h"
#include "socket.h"
#include "threadpool.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct event_info {
    int *kfd;
    struct kevent *self;
    void (*cb_func)(void *);
} event_info;

int ahttpd_event_option(int *kfd, int fd, int filter, int option, void (*func)(void *)) {
    event_info *eb;
    if (option & EV_ADD) {
        eb = malloc(sizeof(event_info));
        eb->cb_func = func;
        eb->kfd = kfd;
        eb->self = malloc(sizeof(struct kevent));
    }
    EV_SET(eb->self, fd, filter, option, 0, 0, eb);
    kevent(*kfd, eb->self, 1, NULL, 0, NULL);
    return 0;
}//ahttpd_event_option
void ahttpd_event_close(event_info *event) {
    ahttpd_close(event->self->ident);
    free(event->self);
}
void ahttpd_thread_read(void *arg) {
    if (arg == NULL)
        return;
    event_info *event = (event_info *) arg;
    //working: http handle
    (event->cb_func)((void *) event->self);
    event->self->data = 0;
    /*
    // todo: keep-alive 
    int kfd = kqueue();
    struct kevent callback;
    struct timespec block_time = {5, 0};
    kevent(kfd, event->self, 1, NULL, 0, NULL);
    int ret = kevent(kfd, NULL, 0, &callback, 1, &block_time);
    if (ret > 0 && (callback.flags & EVFILT_READ)) {
        (event->cb_func)((void *) event->self);
    }
    ahttpd_close(kfd);
    */
    ahttpd_event_close(event);
    ahttpd_log(LOG_INFO, "client close.");

}//ahttpd_thread_read
void ahttpd_thread_accept(void *arg) {
    event_info *event = (event_info *) arg;
    int cfd, j;
    for (j = 0; j < event->self->data; j++) {
        cfd = ahttpd_accept(event->self->ident, NULL, NULL);
        if (cfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                ahttpd_log(LOG_ERR, "accept error. %s", strerror(errno));
                break;
            }
        } else {
            setnoblock(cfd);
            ahttpd_event_option(event->kfd, cfd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, event->cb_func);
        }
    }
    event->self->data = 0;
}//ahttpd_thread_accept
void ahttpd_event_handler(int sockfd, int backlog, void (*func)(void *)) {
    struct kevent events[backlog];
    event_info *dc;
    int kfd = kqueue();
    ahttpd_event_option(&kfd, sockfd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, func);
    int ready, i;
    while (1) {
        ready = kevent(kfd, NULL, 0, events, backlog, NULL);
        if (ready < 0) {
            ahttpd_log(LOG_ERR, "kevent failed. %s", strerror(errno));
            break;
        }
        for (i = 0; i < ready; i++) {
            if (events[i].flags & EV_EOF) {
                ahttpd_event_close((events[i].udata));
            } else if ((events[i].flags & EVFILT_READ)) {
                ((event_info *) (events[i].udata))->self->data = events[i].data;
                if (events[i].ident == sockfd) {
                    ahttpd_thread_accept(events[i].udata);
                } else {
                    ahttpd_threadpool_add(ahttpd_thread_read, events[i].udata);
                }
            }
        }
    }
}//ahttpd_event_handler
