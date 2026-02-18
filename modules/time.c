#include "itsuki_ext.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

static const ItsukiApi* g_api = NULL;

static Result fail(TipoError tipo, const char* msg) {
    if (g_api && g_api->raise) g_api->raise(tipo, msg ? msg : "Error");
    return g_api ? g_api->make_null() : (Result){0};
}

static int is_number(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

static int is_string(Result r) {
    return r.tipo == TIPO_CADENA && r.s != NULL;
}

static double unix_now_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL;
    unsigned long long t100;
    unsigned long long unix100;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    t100 = uli.QuadPart;
    if (t100 < EPOCH_DIFF_100NS) return 0.0;
    unix100 = t100 - EPOCH_DIFF_100NS;
    return (double)(unix100 / 10000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
#endif
}

static double monotonic_now_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        if (freq.QuadPart == 0) return 0.0;
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

static int tm_local(time_t sec, struct tm* out_tm) {
#ifdef _WIN32
    struct tm* p = localtime(&sec);
    if (!p) return 0;
    *out_tm = *p;
    return 1;
#else
    return localtime_r(&sec, out_tm) != NULL;
#endif
}

static int tm_utc(time_t sec, struct tm* out_tm) {
#ifdef _WIN32
    struct tm* p = gmtime(&sec);
    if (!p) return 0;
    *out_tm = *p;
    return 1;
#else
    return gmtime_r(&sec, out_tm) != NULL;
#endif
}

static void sleep_ms_native(double ms) {
    if (ms < 0) ms = 0;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000.0);
    ts.tv_nsec = (long)((ms - (double)ts.tv_sec * 1000.0) * 1000000.0);
    nanosleep(&ts, NULL);
#endif
}

static Result format_epoch_ms(double ms_in, const char* fmt, int utc) {
    time_t sec = (time_t)(ms_in / 1000.0);
    struct tm tmv;
    char buf[256];
    size_t n;

    if (!fmt) fmt = "%Y-%m-%dT%H:%M:%S";

    if (utc) {
        if (!tm_utc(sec, &tmv)) return fail(ERROR_SISTEMA, "time format UTC fallo");
    } else {
        if (!tm_local(sec, &tmv)) return fail(ERROR_SISTEMA, "time format local fallo");
    }

    n = strftime(buf, sizeof(buf), fmt, &tmv);
    if (n == 0) return g_api->make_string("");
    return g_api->make_string(buf);
}

static Result tm_to_map(const struct tm* t) {
    Result m = g_api->make_map();
    g_api->map_set(m, "year", g_api->make_number((double)(t->tm_year + 1900)));
    g_api->map_set(m, "month", g_api->make_number((double)(t->tm_mon + 1)));
    g_api->map_set(m, "day", g_api->make_number((double)t->tm_mday));
    g_api->map_set(m, "hour", g_api->make_number((double)t->tm_hour));
    g_api->map_set(m, "minute", g_api->make_number((double)t->tm_min));
    g_api->map_set(m, "second", g_api->make_number((double)t->tm_sec));
    g_api->map_set(m, "wday", g_api->make_number((double)t->tm_wday)); /* 0=domingo */
    g_api->map_set(m, "yday", g_api->make_number((double)(t->tm_yday + 1)));
    g_api->map_set(m, "isdst", g_api->make_number((double)t->tm_isdst));
    return m;
}

static int64_t days_from_civil(int y, unsigned m, unsigned d) {
    int era;
    unsigned yoe;
    unsigned doy;
    unsigned doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153U * (m + (m > 2 ? (unsigned)-3 : 9U)) + 2U) / 5U + d - 1U;
    doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static int64_t civil_to_epoch_seconds(int year, int month, int day, int hour, int minute, int second) {
    int64_t days = days_from_civil(year, (unsigned)month, (unsigned)day);
    return days * 86400LL + (int64_t)hour * 3600LL + (int64_t)minute * 60LL + (int64_t)second;
}

static int64_t tm_to_epoch_utc(const struct tm* t) {
    return civil_to_epoch_seconds(
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec
    );
}

static int read_datetime_args(Result args[], int n_args, int* y, int* m, int* d, int* hh, int* mm, int* ss) {
    if (n_args < 3 || !is_number(args[0]) || !is_number(args[1]) || !is_number(args[2])) return 0;
    *y = (int)args[0].n;
    *m = (int)args[1].n;
    *d = (int)args[2].n;
    *hh = (n_args > 3 && is_number(args[3])) ? (int)args[3].n : 0;
    *mm = (n_args > 4 && is_number(args[4])) ? (int)args[4].n : 0;
    *ss = (n_args > 5 && is_number(args[5])) ? (int)args[5].n : 0;
    return 1;
}

static Result fn_now_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(unix_now_ms());
}

static Result fn_now_s(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(unix_now_ms() / 1000.0);
}

static Result fn_time(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(unix_now_ms() / 1000.0);
}

static Result fn_monotonic_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(monotonic_now_ms());
}

static Result fn_monotonic_s(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(monotonic_now_ms() / 1000.0);
}

static Result fn_perf_counter_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(monotonic_now_ms());
}

static Result fn_perf_counter_s(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number(monotonic_now_ms() / 1000.0);
}

static Result fn_process_time_s(Result args[], int n_args) {
    clock_t c;
    (void)args;
    (void)n_args;
    c = clock();
    if (c == (clock_t)-1) return fail(ERROR_SISTEMA, "time.process_time_s fallo");
    return g_api->make_number((double)c / (double)CLOCKS_PER_SEC);
}

static Result fn_sleep_ms(Result args[], int n_args) {
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.sleep_ms(ms) requiere numero");
    sleep_ms_native(args[0].n);
    return g_api->make_null();
}

static Result fn_sleep_s(Result args[], int n_args) {
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.sleep_s(s) requiere numero");
    sleep_ms_native(args[0].n * 1000.0);
    return g_api->make_null();
}

static Result fn_format_utc_ms(Result args[], int n_args) {
    double ms;
    const char* fmt = "%Y-%m-%dT%H:%M:%SZ";
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.format_utc_ms(epoch_ms, [fmt])");
    ms = args[0].n;
    if (n_args > 1) {
        if (!is_string(args[1])) return fail(ERROR_ARGUMENTO, "time.format_utc_ms(epoch_ms, [fmt])");
        fmt = args[1].s;
    }
    return format_epoch_ms(ms, fmt, 1);
}

static Result fn_format_local_ms(Result args[], int n_args) {
    double ms;
    const char* fmt = "%Y-%m-%dT%H:%M:%S";
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.format_local_ms(epoch_ms, [fmt])");
    ms = args[0].n;
    if (n_args > 1) {
        if (!is_string(args[1])) return fail(ERROR_ARGUMENTO, "time.format_local_ms(epoch_ms, [fmt])");
        fmt = args[1].s;
    }
    return format_epoch_ms(ms, fmt, 0);
}

static Result fn_strftime(Result args[], int n_args) {
    const char* fmt;
    double ms = unix_now_ms();
    int utc = 0;

    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "time.strftime(fmt, [epoch_ms], [utc])");
    fmt = args[0].s;
    if (n_args > 1) {
        if (!is_number(args[1])) return fail(ERROR_ARGUMENTO, "time.strftime(fmt, [epoch_ms], [utc])");
        ms = args[1].n;
    }
    if (n_args > 2) {
        if (!is_number(args[2])) return fail(ERROR_ARGUMENTO, "time.strftime(fmt, [epoch_ms], [utc])");
        utc = args[2].n != 0 ? 1 : 0;
    }
    return format_epoch_ms(ms, fmt, utc);
}

static Result fn_iso8601_utc_now(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return format_epoch_ms(unix_now_ms(), "%Y-%m-%dT%H:%M:%SZ", 1);
}

static Result fn_localtime_ms(Result args[], int n_args) {
    double ms = unix_now_ms();
    time_t sec;
    struct tm tmv;
    if (n_args > 0) {
        if (!is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.localtime_ms([epoch_ms])");
        ms = args[0].n;
    }
    sec = (time_t)(ms / 1000.0);
    if (!tm_local(sec, &tmv)) return fail(ERROR_SISTEMA, "time.localtime_ms fallo");
    return tm_to_map(&tmv);
}

static Result fn_gmtime_ms(Result args[], int n_args) {
    double ms = unix_now_ms();
    time_t sec;
    struct tm tmv;
    if (n_args > 0) {
        if (!is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.gmtime_ms([epoch_ms])");
        ms = args[0].n;
    }
    sec = (time_t)(ms / 1000.0);
    if (!tm_utc(sec, &tmv)) return fail(ERROR_SISTEMA, "time.gmtime_ms fallo");
    return tm_to_map(&tmv);
}

static Result fn_mktime_local(Result args[], int n_args) {
    int y, m, d, hh, mm, ss;
    struct tm tmv;
    time_t sec;
    if (!read_datetime_args(args, n_args, &y, &m, &d, &hh, &mm, &ss)) {
        return fail(ERROR_ARGUMENTO, "time.mktime_local(year, month, day, [hour], [minute], [second])");
    }
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = m - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = hh;
    tmv.tm_min = mm;
    tmv.tm_sec = ss;
    tmv.tm_isdst = -1;
    sec = mktime(&tmv);
    return g_api->make_number((double)sec * 1000.0);
}

static Result fn_mktime_utc(Result args[], int n_args) {
    int y, m, d, hh, mm, ss;
    int64_t sec;
    if (!read_datetime_args(args, n_args, &y, &m, &d, &hh, &mm, &ss)) {
        return fail(ERROR_ARGUMENTO, "time.mktime_utc(year, month, day, [hour], [minute], [second])");
    }
    sec = civil_to_epoch_seconds(y, m, d, hh, mm, ss);
    return g_api->make_number((double)sec * 1000.0);
}

static Result fn_tz_offset_minutes(Result args[], int n_args) {
    double ms = unix_now_ms();
    time_t sec;
    struct tm lt;
    struct tm gt;
    int64_t local_utc_sec;
    int64_t gm_utc_sec;
    if (n_args > 0) {
        if (!is_number(args[0])) return fail(ERROR_ARGUMENTO, "time.tz_offset_minutes([epoch_ms])");
        ms = args[0].n;
    }
    sec = (time_t)(ms / 1000.0);
    if (!tm_local(sec, &lt) || !tm_utc(sec, &gt)) return fail(ERROR_SISTEMA, "time.tz_offset_minutes fallo");
    local_utc_sec = tm_to_epoch_utc(&lt);
    gm_utc_sec = tm_to_epoch_utc(&gt);
    return g_api->make_number((double)(local_utc_sec - gm_utc_sec) / 60.0);
}

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;
    g_api = api;

    if (!api->export_const(module, "MS_PER_SECOND", api->make_number(1000.0))) return 0;
    if (!api->export_const(module, "SECONDS_PER_DAY", api->make_number(86400.0))) return 0;

    if (!api->export_native(module, "now_ms", fn_now_ms)) return 0;
    if (!api->export_native(module, "now_s", fn_now_s)) return 0;
    if (!api->export_native(module, "time", fn_time)) return 0;
    if (!api->export_native(module, "monotonic_ms", fn_monotonic_ms)) return 0;
    if (!api->export_native(module, "monotonic_s", fn_monotonic_s)) return 0;
    if (!api->export_native(module, "perf_counter_ms", fn_perf_counter_ms)) return 0;
    if (!api->export_native(module, "perf_counter_s", fn_perf_counter_s)) return 0;
    if (!api->export_native(module, "process_time_s", fn_process_time_s)) return 0;
    if (!api->export_native(module, "sleep_ms", fn_sleep_ms)) return 0;
    if (!api->export_native(module, "sleep_s", fn_sleep_s)) return 0;
    if (!api->export_native(module, "format_utc_ms", fn_format_utc_ms)) return 0;
    if (!api->export_native(module, "format_local_ms", fn_format_local_ms)) return 0;
    if (!api->export_native(module, "strftime", fn_strftime)) return 0;
    if (!api->export_native(module, "iso8601_utc_now", fn_iso8601_utc_now)) return 0;
    if (!api->export_native(module, "localtime_ms", fn_localtime_ms)) return 0;
    if (!api->export_native(module, "gmtime_ms", fn_gmtime_ms)) return 0;
    if (!api->export_native(module, "mktime_local", fn_mktime_local)) return 0;
    if (!api->export_native(module, "mktime_utc", fn_mktime_utc)) return 0;
    if (!api->export_native(module, "tz_offset_minutes", fn_tz_offset_minutes)) return 0;

    return 1;
}
