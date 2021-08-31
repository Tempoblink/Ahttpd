#include "../log.h"
#include <sys/fcntl.h>
#include <stdlib.h>


int main(int argc, char const *argv[]) {
    ahttpd_log_create(NULL, NULL);
    ahttpd_log(LOG_INFO, "%s", "creat test.log");

    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    ahttpd_log(LOG_INFO, "%s", "creat test.log");
    return 0;
}
