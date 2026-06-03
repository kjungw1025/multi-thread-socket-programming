#define LOG_PREFIX "client"
#include "logger.h"

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s {IP} {PORT} {APTYPE}\n", argv[0]);
        return 1;
    }

    const char  *ip     = argv[1];
    int         port    = atoi(argv[2]);
    const char  *aptype = argv[3];

    log_open();
    log_write("INFO", "=== client started IP=%s PORT=%d APTYPE=%s ===", ip, port, aptype);
    
    log_write("INFO", "=== client terminated ===");
    log_close();
}