/*
 * path.h - 실행 파일 경로 유틸리티
 */

#ifndef PATH_H
#define PATH_H

#include <string.h>
#include <unistd.h>
#include <libgen.h>     /* dirname() */

#ifdef __APPLE__
#include <mach-o/dyld.h>    /* _NSGetExecutablePath() — macOS 전용 */
#endif

/* -------------------------------------------------------
 * get_exe_dir : 실행 파일이 위치한 디렉토리 경로 반환
 *   macOS : _NSGetExecutablePath()
 *   Linux : /proc/self/exe readlink
 *
 * input params :
 *      char *out_dir : 결과 경로를 저장할 버퍼
 *      int   bufsz   : 버퍼 크기
 * ------------------------------------------------------- */
static inline void get_exe_dir(char *out_dir, int bufsz) {
    char exe_path[512] = {0};

#ifdef __APPLE__
    /* macOS: _NSGetExecutablePath 사용 */
    uint32_t size = (uint32_t)sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        /* 버퍼 부족 시 fallback */
        strncpy(out_dir, ".", bufsz - 1);
        return;
    }
#else
    /* Linux: /proc/self/exe 심볼릭 링크 읽기 */
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len < 0) {
        strncpy(out_dir, ".", bufsz - 1);
        return;
    }
    exe_path[len] = '\0';
#endif

    /* 실행 파일 디렉토리 추출: .../build/server → .../build */
    char exe_copy[512];
    strncpy(exe_copy, exe_path, sizeof(exe_copy) - 1);
    strncpy(out_dir, dirname(exe_copy), bufsz - 1);
}

#endif /* PATH_H */