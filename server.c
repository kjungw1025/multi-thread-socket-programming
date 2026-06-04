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

/* ---------------------------------------
 * recv_thread: 클라이언트 수신
 * --------------------------------------- */
static void *recv_thread(void *arg) {
    ClientArg *carg = (ClientArg * )arg;
    char buf[BUFFER_SIZE];
    int n;

    log_write("INFO", "[RECV] recv_thread started");

    while (!carg->stop) {
        memset(buf, 0, sizeof(buf));
        n = (int)recv(carg->client_sock, buf, sizeof(buf) - 1, 0);

        if (n < 0) {
            if (!carg->stop) {
                log_write("ERROR", "[RECV] recv() failed (errno=%d: %s)", errno, strerror(errno));
            }
            break;
        }
        if (n == 0) {
            log_write("INFO", "[RECV] Client disconnected");
            break;
        }

        log_packet("INFO", buf, n);

        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[ACK] %.*s", n, buf);
        if (send(carg->client_sock, response, (int)strlen(response), 0) < 0) {
            log_write("ERROR", "[RECV] send(ACK) failed (errno=%d: %s)", errno, strerror(errno));
            break;
        }
        log_packet("INFO", response, (int)strlen(response));
    }

    carg->stop = 1;
    log_write("INFO", "[RECV] recv_thread terminated");
    pthread_exit(NULL);
}

/* ---------------------------------------
 * send_thread: 서버 -> 클라이언트 송신
 * 현재 수신 응답은 recv_thread 에서 처리
 * 이 스레드는 서버 주도 메시지 송신 슬롯
 * --------------------------------------- */
static void *send_thread(void *arg) {
    ClientArg *carg = (ClientArg *)arg;

    log_write("INFO", "[SEND] send_thread started");
    
    /* 현재는 recv_thread 가 종료될 때까지 대기만 수행 */
    while (!carg->stop) {
        usleep(100000);     /* 100ms 간격으로 stop flag polling */
    }

    log_write("INFO", "[SEND] send_thread terminated");
    pthread_exit(NULL);
}

static void handle_client(int client_sock, struct sockaddr_in *client_addr) {
    ClientArg *carg = (ClientArg *)malloc(sizeof(ClientArg));
    if (!carg) {
        log_write("ERROR", "ClientArg malloc failed");
        close(client_sock);
        return;
    }

    memset(carg, 0, sizeof(ClientArg));
    carg->client_sock = client_sock;
    carg->stop        = 0;
    carg->client_port = ntohs(client_addr->sin_port);
    inet_ntop(AF_INET, &client_addr->sin_addr, carg->client_ip, sizeof(carg->client_ip));

    log_write("INFO", "Client connected");

    pthread_t tid_send, tid_recv;

    if (pthread_create(&tid_recv, NULL, recv_thread, carg) != 0) {
        log_write("ERROR", "Failed to create recv_thread (errno=%d: %s)", errno, strerror(errno));
        close(client_sock);
        free(carg);
        return;
    }

    if (pthread_create(&tid_send, NULL, send_thread, carg) != 0) {
        log_write("ERROR", "Failed to create send_thread (errno=%d: %s)", errno, strerror(errno));
        carg->stop = 1;
        pthread_join(tid_recv, NULL);
        close(client_sock);
        free(carg);
        return;
    }

    /* -- 스레드를 detach -> 종료 시 자동 자원 해제 -- */
    pthread_detach(tid_recv);
    pthread_detach(tid_send);
}

int main(void) {

    log_open();
    log_write("INFO", "=== server started ===");

    int port = load_server_port(config_path);
    if (port <= 0) {
        log_write("ERROR", "Failed to load port from config.ini — exiting");
        log_close();
        return 1;
    }
    log_write("INFO", "Config loaded: server_port=%d", port);

    /* -- 서버 소켓 생성 -- */
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        log_write("ERROR", "Socket creation failed (errno=%d: %s)",
                  errno, strerror(errno));
        log_close();
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* -- 주소 바인딩 -- */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_write("ERROR", "bind() failed (errno=%d: %s)", errno, strerror(errno));
        close(server_sock);
        log_close();
        return 1;
    }

    /* -- listen -- */
    if (listen(server_sock, BACKLOG) < 0) {
        log_write("ERROR", "listen() failed (errno=%d: %s)", errno, strerror(errno));
        close(server_sock);
        log_close();
        return 1;
    }
    log_write("INFO", "Server listening — backlog=%d", BACKLOG);

    /* -- accept loop -- */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            log_write("ERROR", "accept() failed (errno=%d: %s)", errno, strerror(errno));
            continue;
        }

        handle_client(client_sock, &client_addr);
    }

    close(server_sock);
    log_write("INFO", "=== server terminated ===");
    log_close();

    return 0;
}