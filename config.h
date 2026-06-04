/*
 * config.h - config.ini 파싱 모듈
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>     /* dirname() */

#ifdef __APPLE__
#include <mach-o/dyld.h>    /* _NSGetExecutablePath() — macOS 전용 */
#endif

#define CONFIG_FILE "config.ini"

/* -----------------------------------------------------------
 * get_config_path: 실행 파일 위치 기준으로 config.ini 경로 반환
 *      build/server → ../config.ini (= 프로젝트 루트/config.ini)
 *
 * input params:
 *      char *out_path = 결과 경로를 저장할 버퍼
 *      int bufsz = 버퍼 크기
 * ----------------------------------------------------------- */
static inline void get_config_path(char *out_path, int bufsz) {
    char exe_path[512] = {0};

#ifdef __APPLE__
    /* macOS: _NSGetExecutablePath 사용 */
    uint32_t size = (uint32_t)sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        /* 버퍼 부족 시 fallback */
        snprintf(out_path, bufsz, "../%s", CONFIG_FILE);
        return;
    }
#else
    /* Linux: /proc/self/exe 심볼릭 링크 읽기 */
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len < 0) {
        snprintf(out_path, bufsz, "../%s", CONFIG_FILE);
        return;
    }
    exe_path[len] = '\0';
#endif

    /* 실행 파일이 위치한 디렉토리 추출 후 한 단계 위 경로 조합 */
    /* ex) .../build/server → dir = .../build      */
    /*     → config = .../build/../config.ini      */
    char exe_copy[512];
    strncpy(exe_copy, exe_path, sizeof(exe_copy) - 1);
    char *dir = dirname(exe_copy);
    snprintf(out_path, bufsz, "%s/../%s", dir, CONFIG_FILE);
}

/* -----------------------------------------------------------
 * load_server_port: config.ini 에서 server_port 값 반환
 *
 * input params: const char *config_path = config.ini 경로
 * return: 포트 번호, 실패 시 -1
 * ----------------------------------------------------------- */
static inline int load_server_port(const char *config_path) {
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
            /* 키 양끝 공백 제거 */
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

#endif /* CONFIG_H */