#define LOG_PREFIX "client"
#include "logger.h"

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

/* ---------------------------------------
 * 스레드 공유 인자 구조체
 * --------------------------------------- */
typedef struct {
    int     sock;
    char    aptype[7];      /* NULL 종료 포함 */
    int     stop;           /* 1 이면 스레드 종료 요청 */
} ThreadArg;

/* ---------------------------------------
 * send_thread: stdin -> 서버 송신
 * --------------------------------------- */
static void *send_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    char input[BUFFER_SIZE];
    char packet[BUFFER_SIZE];

    log_write("INFO", "[SEND] send_thread started (APTYPE=%s)", targ->aptype);

    while (!targ->stop) {
        printf("[SEND] Enter message (exit to quit): ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            log_write("INFO", "[SEND] EOF detected - send_thread exiting");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            log_write("INFO", "[SEND] Exit command received - send_thread exiting");
            targ->stop = 1;
            break;
        }

        snprintf(packet, sizeof(packet), "[%s] %s", targ->aptype, input);

        if (send(targ->sock, packet, (int)strlen(packet), 0) < 0) {
            log_write("ERROR", "[SEND] send() failed (errno=%d: %s)", errno, strerror(errno));
            targ->stop = 1;
            break;
        }

        log_packet("INFO", packet, (int)strlen(packet));
    }

    shutdown(targ->sock, SHUT_WR);
    log_write("INFO", "[SEND] send_thread terminated");
    pthread_exit(NULL);
}

/* ---------------------------------------
 * recv_thread: 서버 응답 수신
 * --------------------------------------- */
static void *recv_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    char buf[BUFFER_SIZE];
    int n;

    log_write("INFO", "[RECV] recv_thread started");

    while (!targ->stop) {
        memset(buf, 0, sizeof(buf));
        n = (int)recv(targ->sock, buf, sizeof(buf) - 1, 0);

        if (n < 0) {
            if (!targ->stop) {
                log_write("ERROR", "[RECV] recv() failed (errno=%d: %s)", errno, strerror(errno));
            }
            break;
        }
        if (n == 0) {
            log_write("INFO", "[RECV] Server disconnected");
            break;
        }

        log_packet("INFO", buf, n);
    }

    targ->stop = 1;
    log_write("INFO", "[RECV] recv_thread terminated");
    pthread_exit(NULL);
}

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

    /* -- 소켓 생성 -- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_write("ERROR", "Socket creation failed (errno=%d: %s)", errno, strerror(errno));
        log_close();
        return 1;
    }
    
    /* -- 서버 주소 설정 -- */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family  = AF_INET;
    server_addr.sin_port    = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        log_write("ERROR", "Invalid IP address: %s", ip);
        close(sock);
        log_close();
        return 1;
    }

    /* -- 서버 연결 -- */
    log_write("INFO", "Connecting to server: %s:%d", ip, port);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_write("ERROR", "connect() failed (errno=%d: %s)", errno, strerror(errno));
        close(sock);
        log_close();
        return 1;
    }
    log_write("INFO", "Connected to server: %s:%d", ip, port);

    /* -- 스레드 인자 초기화 -- */
    ThreadArg targ;
    memset(&targ, 0, sizeof(targ));
    targ.sock = sock;
    targ.stop = 0;
    strncpy(targ.aptype, aptype, 6);
    targ.aptype[6] = '\0';

    /* -- send / recv 스레드 생성 -- */
    pthread_t tid_send, tid_recv;

    if (pthread_create(&tid_send, NULL, send_thread, &targ) != 0) {
        log_write("ERROR", "Failed to create send_thread (errno=%d: %s)",
                  errno, strerror(errno));
        close(sock);
        log_close();
        return 1;
    }
    log_write("INFO", "send_thread created");

    if (pthread_create(&tid_recv, NULL, recv_thread, &targ) != 0) {
        log_write("ERROR", "Failed to create recv_thread (errno=%d: %s)",
                  errno, strerror(errno));
        close(sock);
        log_close();
        return 1;
    }
    log_write("INFO", "recv_thread created");

    /* -- 스레드 종료 대기 -- */
    pthread_join(tid_send, NULL);
    pthread_join(tid_recv, NULL);

    log_write("INFO", "=== client terminated ===");
    close(sock);
    log_close();

    return 0;
}