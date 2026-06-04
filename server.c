#define LOG_PREFIX "server"
#include "logger.h"

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define CONFIG_FILE "config.ini"
#define BUFFER_SIZE 4096
#define BACKLOG     5

/* ---------------------------------------
 * 클라이언트별 스레드 인자 구조체
 * --------------------------------------- */
typedef struct {
    int     client_sock;
    char    client_ip[INET_ADDRSTRLEN];
    int     client_port;
    int     stop;
} ClientArg;

/* ---------------------------------------
 * config.ini 파싱: server_port 값 반환
 * return: 포트 번호, 실패 시 -1
 * --------------------------------------- */
static int load_server_port(const char *config_path) {
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] Failed to open config file: %s\n", config_path);
        return -1;
    }

    char line[128];
    int  port = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char key[64], val[64];
        if (sscanf(line, "%63[^=]=%63s", key, val) == 2) {
            char *k = key;
            while (*k == ' ') k++;
            char *end = k + strlen(k) - 1;
            while (end > k && (*end == ' ' || *end == '\r' || *end == '\n'))
                *end-- = '\0';
            
            if (strcmp(k, "server_port") == 0) {
                port = atoi(val);
                break;
            }
        }
    }

    fclose(fp);
    return port;
}

int main() {
    
}