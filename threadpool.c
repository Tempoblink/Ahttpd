#include "threadpool.h"
#include "log.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_ADD 5
#define DEFAULT_DESTROY 5

typedef struct {
    void (*func)(void *arg);
    void *arg;
} _ahttpd_thread_task_t;

typedef struct {
    int min;
    int max;
    int current;
    int working;
    int destroy;//pool_look
    pthread_mutex_t threads_count_lock;
} _ahttpd_threads_count_t;

// dynamic threadpool & static threadpool
typedef struct {
    // threads status
    pthread_t *threads;
    _ahttpd_threads_count_t *threads_count;
    // task queue
    _ahttpd_thread_task_t *tasks;
    int tasks_head;
    int tasks_tail;
    int tasks_count;
    pthread_cond_t tasks_created;
    pthread_cond_t tasks_need;
    //pool status
    pthread_t pool_master;
    pthread_mutex_t pool_lock;
    int pool_tasks;
    int pool_shutdown;
} _ahttpd_threadpool_t;

_ahttpd_threadpool_t *pool;
static pthread_attr_t thread_attr;
void *_ahttpd_thread_worker(void *arg);
void *_ahttpd_thread_master(void *arg);

int _ahttpd_thread_status(pthread_t tid) {
    int status = pthread_kill(tid, 0);
    if (status == ESRCH)
        return 1;
    return 0;
}//_ahttpd_thread_status
int _ahttpd_threadpool_free(void) {
    if (pool == NULL)
        return -1;
    if (pool->threads) {
        free(pool->threads);
    }
    if (pool->threads_count) {
        pthread_mutex_lock(&(pool->pool_lock));
        pthread_mutex_destroy(&(pool->pool_lock));
        pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
        pthread_mutex_destroy(&(pool->threads_count->threads_count_lock));
        free(pool->threads_count);
    }
    if (pool->tasks) {
        free(pool->tasks);
        pthread_cond_destroy(&(pool->tasks_created));
        pthread_cond_destroy(&(pool->tasks_need));
    }
    pthread_attr_destroy(&thread_attr);
    free(pool);
    pool = NULL;
    return 0;
}//_ahttpd_threadpool_free
int ahttpd_threadpool_create(int threads_count_min, int threads_count_max, int tasks_count_max) {
    int i;
    do {
        if ((pool = (_ahttpd_threadpool_t *) malloc(sizeof(_ahttpd_threadpool_t))) == NULL) {
            ahttpd_log(LOG_ERR, "threadpool pool malloc failed. %s", strerror(errno));
            break;
        }
        //thread status
        if ((pool->threads = malloc(sizeof(pthread_t) * threads_count_max)) == NULL) {
            ahttpd_log(LOG_ERR, "threadpool threads malloc failed. %s", strerror(errno));
            break;
        }
        memset(pool->threads, 0x00, sizeof(pthread_t) * threads_count_max);
        if ((pool->threads_count = malloc(sizeof(_ahttpd_threads_count_t))) == NULL) {
            ahttpd_log(LOG_ERR, "threadpool threads_count malloc failed. %s", strerror(errno));
            break;
        }
        if (pthread_mutex_init(&pool->threads_count->threads_count_lock, NULL) != 0) {
            ahttpd_log(LOG_ERR, "threadpool threads_count_lock init failed. %s", strerror(errno));
            break;
        }
        pool->threads_count->min = threads_count_min;
        pool->threads_count->max = threads_count_max;
        pool->threads_count->current = threads_count_min;
        pool->threads_count->working = 0;
        pool->threads_count->destroy = 0;
        // task queue
        if ((pool->tasks = malloc(sizeof(_ahttpd_thread_task_t) * tasks_count_max)) == NULL) {
            ahttpd_log(LOG_ERR, "threadpool tasks malloc failed. %s", strerror(errno));
            break;
        }
        pool->tasks_head = pool->tasks_tail = pool->tasks_count = 0;
        if ((pthread_cond_init(&pool->tasks_created, NULL)) != 0 || pthread_cond_init(&pool->tasks_need, NULL) != 0) {
            ahttpd_log(LOG_ERR, "threadpool cond init failed. %s", strerror(errno));
            break;
        }
        //pthreadpool status
        if (pthread_mutex_init(&pool->pool_lock, NULL) != 0) {
            ahttpd_log(LOG_ERR, "threadpool pool_lock init failed. %s", strerror(errno));
            break;
        }
        pool->pool_tasks = tasks_count_max;
        pool->pool_shutdown = 0;
        //create threads;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        //dynamic or static
        if (threads_count_min < threads_count_max) {
            pthread_create(&(pool->pool_master), &thread_attr, _ahttpd_thread_master, NULL);
            ahttpd_log(LOG_INFO, "create master thread: 0x%x...", (unsigned long long) pool->pool_master);
        } else {
            pool->pool_master = 0;
        }
        for (i = 0; i < threads_count_min; i++) {
            pthread_create(&(pool->threads[i]), &thread_attr, _ahttpd_thread_worker, NULL);
            ahttpd_log(LOG_INFO, "create worker thread: 0x%x...", (unsigned long long) pool->threads[i]);
        }
        return 0;
    } while (0);
    _ahttpd_threadpool_free();
    return -1;
}//ahttpd_threadpool_create
int ahttpd_threadpool_add(void (*func)(void *arg), void *arg) {
    pthread_mutex_lock(&(pool->pool_lock));

    while ((pool->tasks_count == pool->pool_tasks) && (!pool->pool_shutdown)) {
        pthread_cond_wait(&(pool->tasks_need), &(pool->pool_lock));
    }
    if (pool->pool_shutdown) {
        pthread_mutex_unlock(&(pool->pool_lock));
        return -1;
    }
    // add task
    if (pool->tasks[pool->tasks_tail].arg != NULL) {
        pool->tasks[pool->tasks_tail].arg = NULL;
    }
    pool->tasks[pool->tasks_tail].func = func;
    pool->tasks[pool->tasks_tail].arg = arg;
    pool->tasks_tail = (pool->tasks_tail + 1) % pool->pool_tasks;
    pool->tasks_count++;
    ahttpd_log(LOG_INFO, "task add.");

    pthread_cond_signal(&(pool->tasks_created));
    pthread_mutex_unlock(&(pool->pool_lock));
    return 0;
}//ahttpd_threadpool_add
void *_ahttpd_thread_worker(void *arg) {
    _ahttpd_thread_task_t task;
    while (1) {
        pthread_mutex_lock(&(pool->pool_lock));
        while ((pool->threads_count->destroy == 0) && (pool->tasks_count == 0) && (!pool->pool_shutdown)) {
            pthread_cond_wait(&(pool->tasks_created), &(pool->pool_lock));
        }
        if (pool->threads_count->destroy > 0) {
            goto destroyed;
        } else if (pool->tasks_count) {
            task.func = pool->tasks[pool->tasks_head].func;
            task.arg = pool->tasks[pool->tasks_head].arg;
            pool->tasks_head = (pool->tasks_head + 1) % pool->pool_tasks;
            pool->tasks_count--;
            pthread_mutex_unlock(&(pool->pool_lock));
            ahttpd_log(LOG_INFO, "[0x%x]thread get task.", (unsigned long long) pthread_self());
            pthread_cond_signal(&(pool->tasks_need));

            pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
            pool->threads_count->working++;
            pthread_mutex_unlock(&(pool->threads_count->threads_count_lock));

            ahttpd_log(LOG_INFO, "[0x%x]start task.", (unsigned long long) pthread_self());
            ((*task.func))(task.arg);

            pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
            pool->threads_count->working--;
            pthread_mutex_unlock(&(pool->threads_count->threads_count_lock));
        } else if (pool->pool_shutdown) {
            goto destroyed;
        } else {
            pthread_mutex_unlock(&(pool->pool_lock));
        }
    }
    pthread_exit(NULL);
destroyed:
    ahttpd_log(LOG_INFO, "destroy worker thread: 0x%x...", (unsigned long long) pthread_self());
    if (!pool->pool_shutdown)
        pool->threads_count->destroy--;
    pthread_mutex_unlock(&(pool->pool_lock));
    pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
    pool->threads_count->current--;
    pthread_mutex_unlock(&(pool->threads_count->threads_count_lock));
    pthread_exit(NULL);
}//_ahttpd_thread_worker
void *_ahttpd_thread_master(void *arg) {
    int cur_tasks_count, cur_threads_count_current, cur_threads_count_working, i, j, status;
    struct timeval times;
    while (!pool->pool_shutdown) {
        sleep(1);
        pthread_mutex_lock(&(pool->pool_lock));
        cur_tasks_count = pool->tasks_count;
        pthread_mutex_unlock(&(pool->pool_lock));

        pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
        cur_threads_count_current = pool->threads_count->current;
        cur_threads_count_working = pool->threads_count->working;
        pthread_mutex_unlock(&(pool->threads_count->threads_count_lock));
        // add threads
        if ((cur_tasks_count > pool->threads_count->min) && (cur_threads_count_current < pool->threads_count->max)) {
            pthread_mutex_lock(&(pool->threads_count->threads_count_lock));
            for (i = 0, j = 0; i < pool->threads_count->max && j < DEFAULT_ADD; i++) {
                if (pool->threads[i] == 0 || _ahttpd_thread_status(pool->threads[i])) {
                    if ((status = pthread_create(&(pool->threads[i]), &thread_attr, _ahttpd_thread_worker, (void *) pool)) != 0) {
                        ahttpd_log(LOG_ERR, "create thread failed. %s", strerror(status));
                    } else {
                        ahttpd_log(LOG_INFO, "create worker thread: 0x%x...", (unsigned long long) pool->threads[i]);
                        j++;
                        pool->threads_count->current++;
                    }
                }
            }
            pthread_mutex_unlock(&(pool->threads_count->threads_count_lock));
        }
        // destroy threads
        if ((cur_threads_count_working * 2) < cur_threads_count_current && cur_threads_count_current > pool->threads_count->min) {
            pthread_mutex_lock(&(pool->pool_lock));
            pool->threads_count->destroy = DEFAULT_DESTROY;
            pthread_mutex_unlock(&(pool->pool_lock));
            pthread_cond_broadcast(&(pool->tasks_created));//signal DEFAULT_DESTROY times to destroy threads;
        }
    }
    pthread_exit((void *) 0);
}//_ahttpd_thread_master
int ahttpd_threadpool_destroy(void) {
    if (pool == NULL)
        return -1;
    pool->pool_shutdown = 1;
    while (pool->threads_count->current > 0) {
        pthread_cond_broadcast(&(pool->tasks_created));
    }
    _ahttpd_threadpool_free();
    return 0;
}//ahttpd_threadpool_destroy
