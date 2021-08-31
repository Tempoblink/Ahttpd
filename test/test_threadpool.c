#include "../log.h"
#include "../threadpool.h"
#include <stdlib.h>
#include <unistd.h>

#define times 50

void test_func(void *arg) {
    ahttpd_log(LOG_INFO, "[%d]Hello, world.", *(int *) arg);
}

int main(int argc, char const *argv[]) {
    ahttpd_log_create(NULL, NULL);
    ahttpd_threadpool_create(10, 10, 30);
    int arg[times];
    for (int i = 0; i < times; i++) {
        arg[i] = i + 1;
        ahttpd_threadpool_add(test_func, (void *) &(arg[i]));
    }
    sleep(10);
    ahttpd_threadpool_destroy();
    return 0;
}
