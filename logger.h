/*
 * logger.h - 공통 로깅 모듈
 * 로그 파일: {LOG_PREFIX}_{YYYYMMDD}.log
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "path.h"       /* get_exe_dir() */

#ifndef LOG_PREFIX
#define LOG_PREFIX "app"
#endif

static FILE *g_log_fp = NULL;

/* 
 * log_open : 로그 파일 오픈
 */
static inline void log_open(void) {
    char exe_dir[512]  = {0};
    char log_dir[600]  = {0};
    char filename[700] = {0};

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    /* 실행 파일 디렉토리 획득 후 한 단계 위/log 경로 조합  */
    /* ex) .../build → .../build/../log = 프로젝트 루트/log */
    get_exe_dir(exe_dir, sizeof(exe_dir));
    snprintf(log_dir, sizeof(log_dir), "%s/../log", exe_dir);

    /* log/ 디렉토리가 없으면 자동 생성 */
    mkdir(log_dir, 0755);

    snprintf(filename, sizeof(filename), "%s/%s_%04d%02d%02d.log",
             log_dir,
             LOG_PREFIX,
             tm_info->tm_year + 1900,
             tm_info->tm_mon  + 1,
             tm_info->tm_mday);

    g_log_fp = fopen(filename, "a");
    if (!g_log_fp) {
        fprintf(stderr, "[WARN] Failed to open log file: %s\n", filename);
    }
}

/*
 * log_close : 로그 파일 닫기
 */
static inline void log_close(void) {
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}

/*
 * get_timestamp : YYYY-MM-DD HH:MM:SS.mmm
 */
static inline void get_timestamp(char *buf, int bufsz) {
    struct timeval  tv;
    struct tm      *tm_info;

    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    snprintf(buf, bufsz, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm_info->tm_year + 1900,
             tm_info->tm_mon  + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec,
             (int)(tv.tv_usec / 1000));
}

/*
 * log_write : stdout + 파일 동시 출력
 */
static inline void log_write(const char *level, const char *fmt, ...) {
    char    ts[32];
    char    msg[512];
    va_list args;

    get_timestamp(ts, sizeof(ts));

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    printf("[%s][%s] %s\n", ts, level, msg);

    if (g_log_fp) {
        fprintf(g_log_fp, "[%s][%s] %s\n", ts, level, msg);
        fflush(g_log_fp);
    }
}

/*
 * log_packet : 패킷 원문 로그
 */
static inline void log_packet(const char *direction, const char *buf, int len) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));

    printf("[%s][%s] LEN=%d | %.*s\n", ts, direction, len, len, buf);

    if (g_log_fp) {
        fprintf(g_log_fp, "[%s][%s] LEN=%d | %.*s\n", ts, direction, len, len, buf);
        fflush(g_log_fp);
    }
}

#endif /* LOGGER_H */