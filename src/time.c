#include "itsuki.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double unix_now_ms() {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // 100-ns since 1601-01-01 -> ms since 1970-01-01
    const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL;
    unsigned long long t100 = uli.QuadPart;
    if (t100 < EPOCH_DIFF_100NS) return 0.0;
    unsigned long long unix100 = t100 - EPOCH_DIFF_100NS;
    return (double)(unix100 / 10000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
#endif
}

static double monotonic_now_ms() {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        if (freq.QuadPart == 0) return 0.0;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

static Result format_epoch_ms(double ms_in, const char* fmt, int utc) {
    if (!fmt) fmt = "%Y-%m-%dT%H:%M:%S";
    if (ms_in < 0) ms_in = 0;
    time_t sec = (time_t)(ms_in / 1000.0);
    struct tm tmv;
#ifdef _WIN32
    {
        struct tm* pt = utc ? gmtime(&sec) : localtime(&sec);
        if (!pt) memset(&tmv, 0, sizeof(tmv));
        else tmv = *pt;
    }
#else
    if (utc) gmtime_r(&sec, &tmv);
    else localtime_r(&sec, &tmv);
#endif
    char buf[256];
    size_t n = strftime(buf, sizeof(buf), fmt, &tmv);
    if (n == 0) {
        return gc_new_string("");
    }
    return gc_new_string(buf);
}

Result builtin_time_now_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return (Result){.tipo = TIPO_NUMERO, .n = unix_now_ms()};
}

Result builtin_time_monotonic_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return (Result){.tipo = TIPO_NUMERO, .n = monotonic_now_ms()};
}

Result builtin_time_sleep_ms(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "sleep_ms requiere ms");
    double ms = args[0].n;
    if (ms < 0) ms = 0;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000.0);
    ts.tv_nsec = (long)((ms - (double)ts.tv_sec * 1000.0) * 1000000.0);
    nanosleep(&ts, NULL);
#endif
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_time_format_utc_ms(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "format_utc_ms requiere epoch_ms, [fmt]");
    double ms = args[0].n;
    const char* fmt = (n_args > 1 && args[1].tipo == TIPO_CADENA) ? args[1].s : "%Y-%m-%dT%H:%M:%SZ";
    return format_epoch_ms(ms, fmt, 1);
}

Result builtin_time_format_local_ms(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "format_local_ms requiere epoch_ms, [fmt]");
    double ms = args[0].n;
    const char* fmt = (n_args > 1 && args[1].tipo == TIPO_CADENA) ? args[1].s : "%Y-%m-%dT%H:%M:%S";
    return format_epoch_ms(ms, fmt, 0);
}
