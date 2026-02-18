#include "itsuki_ext.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef ERROR_TIMEOUT
#define ERROR_TIMEOUT 1460L
#endif
#ifndef ERROR_GEN_FAILURE
#define ERROR_GEN_FAILURE 31L
#endif

#ifndef CONDITION_VARIABLE
#define ITS_THREADS_LEGACY_WIN 1
typedef struct {
    HANDLE event;
} CONDITION_VARIABLE;

typedef struct {
    CRITICAL_SECTION cs;
} SRWLOCK;

static void InitializeConditionVariable(CONDITION_VARIABLE* cv) {
    cv->event = CreateEventA(NULL, TRUE, FALSE, NULL);
}

static BOOL SleepConditionVariableCS(CONDITION_VARIABLE* cv, CRITICAL_SECTION* cs, DWORD timeout_ms) {
    DWORD wr;
    LeaveCriticalSection(cs);
    wr = WaitForSingleObject(cv->event, timeout_ms);
    EnterCriticalSection(cs);
    if (wr == WAIT_OBJECT_0) {
        ResetEvent(cv->event);
        return TRUE;
    }
    if (wr == WAIT_TIMEOUT) SetLastError(ERROR_TIMEOUT);
    else SetLastError(ERROR_GEN_FAILURE);
    return FALSE;
}

static void WakeConditionVariable(CONDITION_VARIABLE* cv) { SetEvent(cv->event); }
static void WakeAllConditionVariable(CONDITION_VARIABLE* cv) { SetEvent(cv->event); }

static void InitializeSRWLock(SRWLOCK* lock) { InitializeCriticalSection(&lock->cs); }
static void AcquireSRWLockShared(SRWLOCK* lock) { EnterCriticalSection(&lock->cs); }
static void AcquireSRWLockExclusive(SRWLOCK* lock) { EnterCriticalSection(&lock->cs); }
static BOOLEAN TryAcquireSRWLockShared(SRWLOCK* lock) { return (BOOLEAN)(TryEnterCriticalSection(&lock->cs) ? 1 : 0); }
static BOOLEAN TryAcquireSRWLockExclusive(SRWLOCK* lock) { return (BOOLEAN)(TryEnterCriticalSection(&lock->cs) ? 1 : 0); }
static void ReleaseSRWLockShared(SRWLOCK* lock) { LeaveCriticalSection(&lock->cs); }
static void ReleaseSRWLockExclusive(SRWLOCK* lock) { LeaveCriticalSection(&lock->cs); }
#endif
#else
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

#define THR_MAX_THREADS 256
#define THR_MAX_MUTEXES 256
#define THR_MAX_CONDS 256
#define THR_MAX_SEMS 256
#define THR_MAX_RWLOCKS 128
#define THR_MAX_BARRIERS 128
#define THR_MAX_ATOMICS 256

#define THR_STATUS_NEW 0
#define THR_STATUS_RUNNING 1
#define THR_STATUS_DONE 2
#define THR_STATUS_FAILED 3
#define THR_STATUS_CANCELLED 4

#define THR_TASK_NONE 0
#define THR_TASK_SLEEP 1
#define THR_TASK_SYSTEM 2
#define THR_TASK_SUM 3
#define THR_TASK_FIB 4
#define THR_TASK_UDP_SEND 5

static const ItsukiApi* g_api = NULL;

#ifdef _WIN32
typedef SOCKET its_socket_t;
#define ITS_INVALID_SOCKET INVALID_SOCKET
#define ITS_CLOSE_SOCKET closesocket
#else
typedef int its_socket_t;
#define ITS_INVALID_SOCKET (-1)
#define ITS_CLOSE_SOCKET close
#endif

#ifdef _WIN32
static CRITICAL_SECTION g_lock;
static LONG g_lock_state = 0; /* 0=uninit,1=initing,2=ready */
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef struct {
    int used;
    int id;
    int state;
    int detached;
    int joined;
    int cancel_requested;
    int task;
    double result_num;
    int result_code;
    char error_msg[160];
    union {
        struct {
            int64_t ms;
        } sleep_task;
        struct {
            char cmd[512];
        } system_task;
        struct {
            double start;
            double end;
            double step;
        } sum_task;
        struct {
            int64_t n;
        } fib_task;
        struct {
            int use_external_socket;
            int64_t sock;
            int port;
            int64_t count;
            int64_t delay_ms;
            char host[128];
            char payload[512];
        } udp_task;
    } args;
#ifdef _WIN32
    HANDLE handle;
    DWORD os_tid;
#else
    pthread_t handle;
    int handle_valid;
#endif
} ThreadEntry;

typedef struct {
    int used;
    int id;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
} MutexEntry;

typedef struct {
    int used;
    int id;
#ifdef _WIN32
    CONDITION_VARIABLE cv;
#else
    pthread_cond_t cv;
#endif
} CondEntry;

typedef struct {
    int used;
    int id;
#ifdef _WIN32
    HANDLE sem;
    int value_estimate;
#else
    sem_t sem;
#endif
} SemEntry;

typedef struct {
    int used;
    int id;
#ifdef _WIN32
    SRWLOCK lock;
#else
    pthread_rwlock_t lock;
#endif
} RwLockEntry;

typedef struct {
    int used;
    int id;
    int parties;
    int waiting;
    int generation;
#ifdef _WIN32
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
#else
    pthread_mutex_t mtx;
    pthread_cond_t cv;
#endif
} BarrierEntry;

typedef struct {
    int used;
    int id;
    long long value;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
} AtomicEntry;

static ThreadEntry g_threads[THR_MAX_THREADS];
static MutexEntry g_mutexes[THR_MAX_MUTEXES];
static CondEntry g_conds[THR_MAX_CONDS];
static SemEntry g_sems[THR_MAX_SEMS];
static RwLockEntry g_rwlocks[THR_MAX_RWLOCKS];
static BarrierEntry g_barriers[THR_MAX_BARRIERS];
static AtomicEntry g_atomics[THR_MAX_ATOMICS];

static int g_next_tid = 1;
static int g_next_mid = 1;
static int g_next_cid = 1;
static int g_next_sid = 1;
static int g_next_rid = 1;
static int g_next_bid = 1;
static int g_next_aid = 1;

static void runtime_init(void) {
#ifdef _WIN32
    LONG s = g_lock_state;
    if (s == 2) return;
    if (InterlockedCompareExchange(&g_lock_state, 1, 0) == 0) {
        InitializeCriticalSection(&g_lock);
        InterlockedExchange(&g_lock_state, 2);
    } else {
        while (InterlockedCompareExchange(&g_lock_state, 2, 2) != 2) {
            Sleep(0);
        }
    }
#endif
}

static void lock_all(void) {
    runtime_init();
#ifdef _WIN32
    EnterCriticalSection(&g_lock);
#else
    pthread_mutex_lock(&g_lock);
#endif
}

static void unlock_all(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_lock);
#else
    pthread_mutex_unlock(&g_lock);
#endif
}

static int is_number(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

static Result fail(TipoError tipo, const char* msg) {
    if (g_api && g_api->raise) g_api->raise(tipo, msg ? msg : "Error");
    return g_api ? g_api->make_null() : (Result){0};
}

static int64_t now_ms(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
#endif
}

static void sleep_ms_native(int64_t ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000);
    nanosleep(&req, NULL);
#endif
}

static const char* task_name(int t) {
    switch (t) {
        case THR_TASK_SLEEP: return "sleep";
        case THR_TASK_SYSTEM: return "system";
        case THR_TASK_SUM: return "sum";
        case THR_TASK_FIB: return "fibonacci";
        case THR_TASK_UDP_SEND: return "udp_send";
        default: return "none";
    }
}

static ThreadEntry* find_thread(int id) {
    int i;
    for (i = 0; i < THR_MAX_THREADS; i++) {
        if (g_threads[i].used && g_threads[i].id == id) return &g_threads[i];
    }
    return NULL;
}

static ThreadEntry* alloc_thread(void) {
    int i;
    for (i = 0; i < THR_MAX_THREADS; i++) {
        if (!g_threads[i].used) {
            memset(&g_threads[i], 0, sizeof(ThreadEntry));
            g_threads[i].used = 1;
            g_threads[i].id = g_next_tid++;
            g_threads[i].state = THR_STATUS_NEW;
            return &g_threads[i];
        }
    }
    return NULL;
}

static void cleanup_detached_finished(void) {
    int i;
    for (i = 0; i < THR_MAX_THREADS; i++) {
        ThreadEntry* e = &g_threads[i];
        if (!e->used) continue;
        if (!e->detached) continue;
        if (e->state == THR_STATUS_RUNNING || e->state == THR_STATUS_NEW) continue;
#ifdef _WIN32
        if (e->handle) {
            CloseHandle(e->handle);
            e->handle = NULL;
        }
#else
        if (e->handle_valid && !e->joined) {
            pthread_join(e->handle, NULL);
            e->joined = 1;
            e->handle_valid = 0;
        }
#endif
        memset(e, 0, sizeof(ThreadEntry));
    }
}

static Result build_thread_info(ThreadEntry* e) {
    Result m = g_api->make_map();
    g_api->map_set(m, "id", g_api->make_number((double)e->id));
    g_api->map_set(m, "status", g_api->make_number((double)e->state));
    g_api->map_set(m, "task", g_api->make_string(task_name(e->task)));
    g_api->map_set(m, "result_num", g_api->make_number(e->result_num));
    g_api->map_set(m, "result_code", g_api->make_number((double)e->result_code));
    g_api->map_set(m, "detached", g_api->make_bool(e->detached ? 1 : 0));
    g_api->map_set(m, "cancel_requested", g_api->make_bool(e->cancel_requested ? 1 : 0));
    if (e->error_msg[0]) {
        g_api->map_set(m, "error", g_api->make_string(e->error_msg));
    } else {
        g_api->map_set(m, "error", g_api->make_null());
    }
    return m;
}

#ifdef _WIN32
static DWORD WINAPI thread_worker(void* p)
#else
static void* thread_worker(void* p)
#endif
{
    ThreadEntry* e = (ThreadEntry*)p;
    e->state = THR_STATUS_RUNNING;
    e->error_msg[0] = '\0';

    if (e->task == THR_TASK_SLEEP) {
        int64_t rem = e->args.sleep_task.ms;
        while (rem > 0) {
            if (e->cancel_requested) {
                e->state = THR_STATUS_CANCELLED;
#ifdef _WIN32
                return 0;
#else
                return NULL;
#endif
            }
            if (rem > 20) {
                sleep_ms_native(20);
                rem -= 20;
            } else {
                sleep_ms_native(rem);
                rem = 0;
            }
        }
        e->result_num = 0.0;
        e->result_code = 0;
        e->state = THR_STATUS_DONE;
    } else if (e->task == THR_TASK_SYSTEM) {
        int rc = system(e->args.system_task.cmd);
        e->result_code = rc;
        e->result_num = (double)rc;
        e->state = THR_STATUS_DONE;
    } else if (e->task == THR_TASK_SUM) {
        double x = e->args.sum_task.start;
        double end = e->args.sum_task.end;
        double step = e->args.sum_task.step;
        double acc = 0.0;
        long long guard = 0;
        if (step == 0.0) {
            snprintf(e->error_msg, sizeof(e->error_msg), "step no puede ser 0");
            e->result_code = -1;
            e->state = THR_STATUS_FAILED;
#ifdef _WIN32
            return 0;
#else
            return NULL;
#endif
        }
        if (step > 0) {
            while (x <= end) {
                if (e->cancel_requested) {
                    e->state = THR_STATUS_CANCELLED;
#ifdef _WIN32
                    return 0;
#else
                    return NULL;
#endif
                }
                acc += x;
                x += step;
                if (++guard > 100000000) break;
            }
        } else {
            while (x >= end) {
                if (e->cancel_requested) {
                    e->state = THR_STATUS_CANCELLED;
#ifdef _WIN32
                    return 0;
#else
                    return NULL;
#endif
                }
                acc += x;
                x += step;
                if (++guard > 100000000) break;
            }
        }
        e->result_num = acc;
        e->result_code = 0;
        e->state = THR_STATUS_DONE;
    } else if (e->task == THR_TASK_FIB) {
        int64_t n = e->args.fib_task.n;
        uint64_t a = 0;
        uint64_t b = 1;
        uint64_t c = 0;
        int64_t i;
        if (n < 0 || n > 93) {
            snprintf(e->error_msg, sizeof(e->error_msg), "n fuera de rango (0..93)");
            e->result_code = -1;
            e->state = THR_STATUS_FAILED;
#ifdef _WIN32
            return 0;
#else
            return NULL;
#endif
        }
        if (n == 0) {
            e->result_num = 0;
            e->result_code = 0;
            e->state = THR_STATUS_DONE;
#ifdef _WIN32
            return 0;
#else
            return NULL;
#endif
        }
        if (n == 1) {
            e->result_num = 1;
            e->result_code = 0;
            e->state = THR_STATUS_DONE;
#ifdef _WIN32
            return 0;
#else
            return NULL;
#endif
        }
        for (i = 2; i <= n; i++) {
            if (e->cancel_requested) {
                e->state = THR_STATUS_CANCELLED;
#ifdef _WIN32
                return 0;
#else
                return NULL;
#endif
            }
            c = a + b;
            a = b;
            b = c;
        }
        e->result_num = (double)b;
        e->result_code = 0;
        e->state = THR_STATUS_DONE;
    } else if (e->task == THR_TASK_UDP_SEND) {
        its_socket_t sock = ITS_INVALID_SOCKET;
        int own_socket = 0;
        int sent_ok = 0;
        int sent_fail = 0;
        int64_t i;
        struct sockaddr_in dest;
#ifdef _WIN32
        WSADATA wsa_data;
        int wsa_started = 0;
#endif

#ifdef _WIN32
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            snprintf(e->error_msg, sizeof(e->error_msg), "WSAStartup fallo");
            e->result_code = -1;
            e->state = THR_STATUS_FAILED;
            return 0;
        }
        wsa_started = 1;
#endif

        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons((unsigned short)e->args.udp_task.port);
#ifdef _WIN32
        {
            unsigned long ip = inet_addr(e->args.udp_task.host);
            if (ip == INADDR_NONE && strcmp(e->args.udp_task.host, "255.255.255.255") != 0) {
                snprintf(e->error_msg, sizeof(e->error_msg), "host UDP invalido (usar IPv4)");
                e->result_code = -1;
                e->state = THR_STATUS_FAILED;
                goto udp_done;
            }
            dest.sin_addr.s_addr = ip;
        }
#else
        if (inet_pton(AF_INET, e->args.udp_task.host, &dest.sin_addr) != 1) {
            snprintf(e->error_msg, sizeof(e->error_msg), "host UDP invalido (usar IPv4)");
            e->result_code = -1;
            e->state = THR_STATUS_FAILED;
            goto udp_done;
        }
#endif

        if (e->args.udp_task.use_external_socket) {
            sock = (its_socket_t)(uintptr_t)e->args.udp_task.sock;
        } else {
            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            own_socket = 1;
        }

        if (sock == ITS_INVALID_SOCKET) {
            snprintf(e->error_msg, sizeof(e->error_msg), "socket UDP fallo");
            e->result_code = -1;
            e->state = THR_STATUS_FAILED;
            goto udp_done;
        }

        for (i = 0; i < e->args.udp_task.count; i++) {
            int r;
            if (e->cancel_requested) {
                e->result_num = (double)sent_ok;
                e->result_code = sent_fail;
                e->state = THR_STATUS_CANCELLED;
                goto udp_done;
            }
            r = (int)sendto(sock,
                            e->args.udp_task.payload,
                            (int)strlen(e->args.udp_task.payload),
                            0,
                            (struct sockaddr*)&dest,
                            (socklen_t)sizeof(dest));
            if (r >= 0) sent_ok++;
            else sent_fail++;
            if (e->args.udp_task.delay_ms > 0) sleep_ms_native(e->args.udp_task.delay_ms);
        }

        e->result_num = (double)sent_ok;
        e->result_code = sent_fail;
        e->state = THR_STATUS_DONE;

udp_done:
        if (own_socket && sock != ITS_INVALID_SOCKET) ITS_CLOSE_SOCKET(sock);
#ifdef _WIN32
        if (wsa_started) WSACleanup();
#endif
    } else {
        snprintf(e->error_msg, sizeof(e->error_msg), "tarea desconocida");
        e->state = THR_STATUS_FAILED;
        e->result_code = -1;
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static Result spawn_task(ThreadEntry* e) {
#ifdef _WIN32
    e->handle = CreateThread(NULL, 0, thread_worker, e, 0, &e->os_tid);
    if (!e->handle) {
        e->used = 0;
        return fail(ERROR_SISTEMA, "CreateThread fallo");
    }
#else
    if (pthread_create(&e->handle, NULL, thread_worker, e) != 0) {
        e->used = 0;
        return fail(ERROR_SISTEMA, "pthread_create fallo");
    }
    e->handle_valid = 1;
#endif
    return g_api->make_number((double)e->id);
}

static int parse_id(Result r, int* out) {
    if (!is_number(r)) return 0;
    *out = (int)r.n;
    return 1;
}

/* -------------------- Thread API -------------------- */

static Result fn_version(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_string("threads/1.1");
}

static Result fn_hw_threads(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return g_api->make_number((double)si.dwNumberOfProcessors);
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    return g_api->make_number((double)n);
#endif
}

static Result fn_now_ms(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_number((double)now_ms());
}

static Result fn_sleep_ms(Result args[], int n_args) {
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "threads.sleep_ms(ms) requiere numero");
    if (args[0].n > 0) sleep_ms_native((int64_t)args[0].n);
    return g_api->make_null();
}

static Result fn_yield_now(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
    return g_api->make_null();
}

static Result fn_spawn_sleep(Result args[], int n_args) {
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !is_number(args[0])) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.spawn_sleep(ms) requiere numero");
    }
    e = alloc_thread();
    if (!e) {
        unlock_all();
        return fail(ERROR_SISTEMA, "limite de threads alcanzado");
    }
    e->task = THR_TASK_SLEEP;
    e->args.sleep_task.ms = (int64_t)args[0].n;
    unlock_all();
    return spawn_task(e);
}

static Result spawn_system_cmd(const char* cmd) {
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    e = alloc_thread();
    if (!e) {
        unlock_all();
        return fail(ERROR_SISTEMA, "limite de threads alcanzado");
    }
    e->task = THR_TASK_SYSTEM;
    strncpy(e->args.system_task.cmd, cmd, sizeof(e->args.system_task.cmd) - 1);
    e->args.system_task.cmd[sizeof(e->args.system_task.cmd) - 1] = '\0';
    unlock_all();
    return spawn_task(e);
}

static Result fn_spawn_system(Result args[], int n_args) {
    if (n_args < 1 || args[0].tipo != TIPO_CADENA || !args[0].s) {
        return fail(ERROR_ARGUMENTO, "threads.spawn_system(cmd) requiere string");
    }
    return spawn_system_cmd(args[0].s);
}

static Result fn_spawn_process(Result args[], int n_args) {
    return fn_spawn_system(args, n_args);
}

static Result fn_spawn_itsuki(Result args[], int n_args) {
    const char* script;
    const char* extra = "";
    const char* exe_path =
#ifdef _WIN32
        "itsuki.exe";
#else
        "./itsuki";
#endif
    char cmd[640];
#ifdef _WIN32
    char script_norm[320];
    char exe_norm[320];
    char script_abs[320];
    char exe_abs[320];
    int i;
#endif

    if (n_args < 1 || args[0].tipo != TIPO_CADENA || !args[0].s) {
        return fail(ERROR_ARGUMENTO, "threads.spawn_itsuki(script, [args], [exe_path])");
    }
    script = args[0].s;
    if (n_args >= 2) {
        if (args[1].tipo != TIPO_CADENA || !args[1].s) {
            return fail(ERROR_ARGUMENTO, "threads.spawn_itsuki: args debe ser string");
        }
        extra = args[1].s;
    }
    if (n_args >= 3) {
        if (args[2].tipo != TIPO_CADENA || !args[2].s) {
            return fail(ERROR_ARGUMENTO, "threads.spawn_itsuki: exe_path debe ser string");
        }
        exe_path = args[2].s;
    }

#ifdef _WIN32
    strncpy(script_norm, script, sizeof(script_norm) - 1);
    script_norm[sizeof(script_norm) - 1] = '\0';
    for (i = 0; script_norm[i]; i++) if (script_norm[i] == '/') script_norm[i] = '\\';
    strncpy(exe_norm, exe_path, sizeof(exe_norm) - 1);
    exe_norm[sizeof(exe_norm) - 1] = '\0';
    for (i = 0; exe_norm[i]; i++) if (exe_norm[i] == '/') exe_norm[i] = '\\';
    if (_fullpath(script_abs, script_norm, sizeof(script_abs)) != NULL) script = script_abs;
    else script = script_norm;
    if (_fullpath(exe_abs, exe_norm, sizeof(exe_abs)) != NULL) exe_path = exe_abs;
    else exe_path = exe_norm;
#endif

    if (extra[0]) {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "call \"%s\" \"%s\" %s", exe_path, script, extra);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" %s", exe_path, script, extra);
#endif
    } else {
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "call \"%s\" \"%s\"", exe_path, script);
#else
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", exe_path, script);
#endif
    }
    cmd[sizeof(cmd) - 1] = '\0';
    return spawn_system_cmd(cmd);
}

static Result fn_spawn_sum(Result args[], int n_args) {
    ThreadEntry* e;
    if (n_args < 2 || !is_number(args[0]) || !is_number(args[1])) {
        return fail(ERROR_ARGUMENTO, "threads.spawn_sum(inicio, fin, [paso]) requiere numeros");
    }
    lock_all();
    cleanup_detached_finished();
    e = alloc_thread();
    if (!e) {
        unlock_all();
        return fail(ERROR_SISTEMA, "limite de threads alcanzado");
    }
    e->task = THR_TASK_SUM;
    e->args.sum_task.start = args[0].n;
    e->args.sum_task.end = args[1].n;
    e->args.sum_task.step = (n_args >= 3 && is_number(args[2])) ? args[2].n : 1.0;
    unlock_all();
    return spawn_task(e);
}

static Result fn_spawn_fib(Result args[], int n_args) {
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !is_number(args[0])) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.spawn_fibonacci(n) requiere numero");
    }
    e = alloc_thread();
    if (!e) {
        unlock_all();
        return fail(ERROR_SISTEMA, "limite de threads alcanzado");
    }
    e->task = THR_TASK_FIB;
    e->args.fib_task.n = (int64_t)args[0].n;
    unlock_all();
    return spawn_task(e);
}

static Result fn_spawn_udp_sender(Result args[], int n_args) {
    ThreadEntry* e;
    int offset = 0;
    int use_external_socket = 0;
    int64_t ext_sock = 0;
    int64_t count;
    int64_t delay_ms = 0;
    int port;
    const char* host;
    const char* payload;

    if (n_args >= 5 && (args[0].tipo == TIPO_SOCKET || is_number(args[0]))) {
        use_external_socket = 1;
        if (args[0].tipo == TIPO_SOCKET) {
            ObjSocket* so = (ObjSocket*)args[0].obj;
            if (!so || !so->is_open) {
                return fail(ERROR_ARGUMENTO, "socket invalido o cerrado");
            }
            ext_sock = (int64_t)(uintptr_t)so->handle;
        } else {
            ext_sock = (int64_t)args[0].n;
        }
        offset = 1;
    }

    if (n_args < (offset + 4) ||
        args[offset].tipo != TIPO_CADENA || !args[offset].s ||
        !is_number(args[offset + 1]) ||
        args[offset + 2].tipo != TIPO_CADENA || !args[offset + 2].s ||
        !is_number(args[offset + 3])) {
        return fail(
            ERROR_ARGUMENTO,
            "threads.spawn_udp_sender([socket], host, port, payload, count, [delay_ms])"
        );
    }

    if (n_args > (offset + 4) && !is_number(args[offset + 4])) {
        return fail(
            ERROR_ARGUMENTO,
            "threads.spawn_udp_sender([socket], host, port, payload, count, [delay_ms])"
        );
    }

    host = args[offset].s;
    port = (int)args[offset + 1].n;
    payload = args[offset + 2].s;
    count = (int64_t)args[offset + 3].n;
    if (n_args > (offset + 4)) delay_ms = (int64_t)args[offset + 4].n;

    if (port <= 0 || port > 65535) return fail(ERROR_ARGUMENTO, "port fuera de rango (1..65535)");
    if (count < 0) return fail(ERROR_ARGUMENTO, "count debe ser >= 0");
    if (delay_ms < 0) delay_ms = 0;

    lock_all();
    cleanup_detached_finished();
    e = alloc_thread();
    if (!e) {
        unlock_all();
        return fail(ERROR_SISTEMA, "limite de threads alcanzado");
    }

    e->task = THR_TASK_UDP_SEND;
    e->args.udp_task.use_external_socket = use_external_socket;
    e->args.udp_task.sock = ext_sock;
    e->args.udp_task.port = port;
    e->args.udp_task.count = count;
    e->args.udp_task.delay_ms = delay_ms;
    strncpy(e->args.udp_task.host, host, sizeof(e->args.udp_task.host) - 1);
    e->args.udp_task.host[sizeof(e->args.udp_task.host) - 1] = '\0';
    strncpy(e->args.udp_task.payload, payload, sizeof(e->args.udp_task.payload) - 1);
    e->args.udp_task.payload[sizeof(e->args.udp_task.payload) - 1] = '\0';
    unlock_all();
    return spawn_task(e);
}

static Result fn_status(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.status(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    unlock_all();
    return g_api->make_number((double)e->state);
}

static Result fn_is_running(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.is_running(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    unlock_all();
    return g_api->make_bool(e->state == THR_STATUS_RUNNING ? 1 : 0);
}

static Result fn_cancel(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.cancel(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    e->cancel_requested = 1;
    unlock_all();
    return g_api->make_bool(1);
}

static Result fn_detach(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.detach(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    if (e->joined) {
        unlock_all();
        return fail(ERROR_EJECUCION, "thread ya fue join");
    }
    if (!e->detached) {
#ifdef _WIN32
        if (e->handle) {
            CloseHandle(e->handle);
            e->handle = NULL;
        }
#else
        if (e->handle_valid) {
            pthread_detach(e->handle);
        }
#endif
        e->detached = 1;
    }
    unlock_all();
    return g_api->make_bool(1);
}

static int thread_done(const ThreadEntry* e) {
    return (e->state != THR_STATUS_NEW && e->state != THR_STATUS_RUNNING);
}

static Result fn_join(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.join(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    if (e->detached) {
        unlock_all();
        return fail(ERROR_EJECUCION, "no se puede join sobre thread detached");
    }
    unlock_all();

    if (!e->joined) {
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(e->handle, INFINITE);
        if (wr != WAIT_OBJECT_0) return fail(ERROR_SISTEMA, "WaitForSingleObject fallo");
        CloseHandle(e->handle);
        e->handle = NULL;
#else
        if (e->handle_valid) {
            if (pthread_join(e->handle, NULL) != 0) return fail(ERROR_SISTEMA, "pthread_join fallo");
            e->handle_valid = 0;
        }
#endif
        e->joined = 1;
    }

    return build_thread_info(e);
}

static Result fn_join_timeout(Result args[], int n_args) {
    int id;
    int64_t timeout_ms;
    ThreadEntry* e;
    int done = 0;
    Result m;

    if (n_args < 2 || !parse_id(args[0], &id) || !is_number(args[1])) {
        return fail(ERROR_ARGUMENTO, "threads.join_timeout(id, timeout_ms)");
    }
    timeout_ms = (int64_t)args[1].n;
    if (timeout_ms < 0) timeout_ms = 0;

    lock_all();
    cleanup_detached_finished();
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    if (e->detached) {
        unlock_all();
        return fail(ERROR_EJECUCION, "no se puede join sobre thread detached");
    }
    unlock_all();

    if (!e->joined) {
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(e->handle, (DWORD)timeout_ms);
        if (wr == WAIT_OBJECT_0) {
            CloseHandle(e->handle);
            e->handle = NULL;
            e->joined = 1;
        } else if (wr == WAIT_TIMEOUT) {
            done = 0;
        } else {
            return fail(ERROR_SISTEMA, "WaitForSingleObject fallo");
        }
#else
        int64_t t0 = now_ms();
        while (!thread_done(e) && (now_ms() - t0) < timeout_ms) {
            sleep_ms_native(1);
        }
        if (thread_done(e) && e->handle_valid) {
            if (pthread_join(e->handle, NULL) != 0) return fail(ERROR_SISTEMA, "pthread_join fallo");
            e->handle_valid = 0;
            e->joined = 1;
        }
#endif
    }

    done = thread_done(e);
    m = g_api->make_map();
    g_api->map_set(m, "done", g_api->make_bool(done ? 1 : 0));
    if (done) {
        g_api->map_set(m, "id", g_api->make_number((double)e->id));
        g_api->map_set(m, "status", g_api->make_number((double)e->state));
        g_api->map_set(m, "task", g_api->make_string(task_name(e->task)));
        g_api->map_set(m, "result_num", g_api->make_number(e->result_num));
        g_api->map_set(m, "result_code", g_api->make_number((double)e->result_code));
        g_api->map_set(m, "detached", g_api->make_bool(e->detached ? 1 : 0));
    }
    return m;
}

static Result fn_try_join(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    int done = 0;
    Result m;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.try_join(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return fail(ERROR_NOMBRE, "thread id no existe");
    }
    done = thread_done(e);
    unlock_all();

    m = g_api->make_map();
    g_api->map_set(m, "done", g_api->make_bool(done ? 1 : 0));
    if (!done) return m;

    if (!e->detached && !e->joined) {
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(e->handle, 0);
        if (wr == WAIT_OBJECT_0) {
            CloseHandle(e->handle);
            e->handle = NULL;
            e->joined = 1;
        }
#else
        if (e->handle_valid) {
            pthread_join(e->handle, NULL);
            e->handle_valid = 0;
            e->joined = 1;
        }
#endif
    }

    g_api->map_set(m, "id", g_api->make_number((double)e->id));
    g_api->map_set(m, "status", g_api->make_number((double)e->state));
    g_api->map_set(m, "task", g_api->make_string(task_name(e->task)));
    g_api->map_set(m, "result_num", g_api->make_number(e->result_num));
    g_api->map_set(m, "result_code", g_api->make_number((double)e->result_code));
    g_api->map_set(m, "detached", g_api->make_bool(e->detached ? 1 : 0));
    return m;
}

static Result fn_destroy_thread(Result args[], int n_args) {
    int id;
    ThreadEntry* e;
    lock_all();
    cleanup_detached_finished();
    if (n_args < 1 || !parse_id(args[0], &id)) {
        unlock_all();
        return fail(ERROR_ARGUMENTO, "threads.destroy(id) requiere id");
    }
    e = find_thread(id);
    if (!e) {
        unlock_all();
        return g_api->make_bool(0);
    }
    if (e->state == THR_STATUS_RUNNING || e->state == THR_STATUS_NEW) {
        unlock_all();
        return fail(ERROR_EJECUCION, "thread aun esta corriendo");
    }
    if (!e->detached && !e->joined) {
        unlock_all();
        return fail(ERROR_EJECUCION, "haz join o detach antes de destroy");
    }
#ifdef _WIN32
    if (e->handle) {
        CloseHandle(e->handle);
        e->handle = NULL;
    }
#endif
    memset(e, 0, sizeof(ThreadEntry));
    unlock_all();
    return g_api->make_bool(1);
}

static Result fn_cleanup_threads(Result args[], int n_args) {
    int before = 0;
    int after = 0;
    int i;
    (void)args;
    (void)n_args;
    lock_all();
    for (i = 0; i < THR_MAX_THREADS; i++) if (g_threads[i].used) before++;
    cleanup_detached_finished();
    for (i = 0; i < THR_MAX_THREADS; i++) if (g_threads[i].used) after++;
    unlock_all();
    return g_api->make_number((double)(before - after));
}

static Result fn_count_threads(Result args[], int n_args) {
    int i;
    int running_only = 0;
    int count = 0;
    if (n_args >= 1 && is_number(args[0])) {
        running_only = ((int)args[0].n != 0);
    }
    lock_all();
    cleanup_detached_finished();
    for (i = 0; i < THR_MAX_THREADS; i++) {
        if (!g_threads[i].used) continue;
        if (running_only && !thread_done(&g_threads[i])) count++;
        else if (!running_only) count++;
    }
    unlock_all();
    return g_api->make_number((double)count);
}

static Result fn_list_threads(Result args[], int n_args) {
    int i;
    int running_only = 0;
    Result arr = g_api->make_array();
    if (n_args >= 1 && is_number(args[0])) {
        running_only = ((int)args[0].n != 0);
    }
    lock_all();
    cleanup_detached_finished();
    for (i = 0; i < THR_MAX_THREADS; i++) {
        Result info;
        if (!g_threads[i].used) continue;
        if (running_only && thread_done(&g_threads[i])) continue;
        info = build_thread_info(&g_threads[i]);
        g_api->array_push(arr, info);
    }
    unlock_all();
    return arr;
}

/* -------------------- Mutex API -------------------- */

static MutexEntry* find_mutex(int id) {
    int i;
    for (i = 0; i < THR_MAX_MUTEXES; i++) if (g_mutexes[i].used && g_mutexes[i].id == id) return &g_mutexes[i];
    return NULL;
}

static Result fn_mutex_new(Result args[], int n_args) {
    int i;
    (void)args;
    (void)n_args;
    lock_all();
    for (i = 0; i < THR_MAX_MUTEXES; i++) {
        if (!g_mutexes[i].used) {
            g_mutexes[i].used = 1;
            g_mutexes[i].id = g_next_mid++;
#ifdef _WIN32
            InitializeCriticalSection(&g_mutexes[i].cs);
#else
            pthread_mutex_init(&g_mutexes[i].mtx, NULL);
#endif
            unlock_all();
            return g_api->make_number((double)g_mutexes[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de mutexes alcanzado");
}

static Result fn_mutex_lock(Result args[], int n_args) {
    int id;
    MutexEntry* m;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.mutex_lock(id)");
    lock_all();
    m = find_mutex(id);
    unlock_all();
    if (!m) return fail(ERROR_NOMBRE, "mutex id no existe");
#ifdef _WIN32
    EnterCriticalSection(&m->cs);
#else
    pthread_mutex_lock(&m->mtx);
#endif
    return g_api->make_bool(1);
}

static Result fn_mutex_try_lock(Result args[], int n_args) {
    int id;
    MutexEntry* m;
    int ok = 0;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.mutex_try_lock(id)");
    lock_all();
    m = find_mutex(id);
    unlock_all();
    if (!m) return fail(ERROR_NOMBRE, "mutex id no existe");
#ifdef _WIN32
    ok = TryEnterCriticalSection(&m->cs) ? 1 : 0;
#else
    ok = (pthread_mutex_trylock(&m->mtx) == 0) ? 1 : 0;
#endif
    return g_api->make_bool(ok);
}

static Result fn_mutex_unlock(Result args[], int n_args) {
    int id;
    MutexEntry* m;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.mutex_unlock(id)");
    lock_all();
    m = find_mutex(id);
    unlock_all();
    if (!m) return fail(ERROR_NOMBRE, "mutex id no existe");
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->mtx);
#endif
    return g_api->make_bool(1);
}

static Result fn_mutex_destroy(Result args[], int n_args) {
    int id;
    MutexEntry* m;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.mutex_destroy(id)");
    lock_all();
    m = find_mutex(id);
    if (!m) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifdef _WIN32
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->mtx);
#endif
    memset(m, 0, sizeof(MutexEntry));
    unlock_all();
    return g_api->make_bool(1);
}

/* -------------------- RWLock API -------------------- */

static RwLockEntry* find_rwlock(int id) {
    int i;
    for (i = 0; i < THR_MAX_RWLOCKS; i++) if (g_rwlocks[i].used && g_rwlocks[i].id == id) return &g_rwlocks[i];
    return NULL;
}

static Result fn_rwlock_new(Result args[], int n_args) {
    int i;
    (void)args;
    (void)n_args;
    lock_all();
    for (i = 0; i < THR_MAX_RWLOCKS; i++) {
        if (!g_rwlocks[i].used) {
            g_rwlocks[i].used = 1;
            g_rwlocks[i].id = g_next_rid++;
#ifdef _WIN32
            InitializeSRWLock(&g_rwlocks[i].lock);
#else
            pthread_rwlock_init(&g_rwlocks[i].lock, NULL);
#endif
            unlock_all();
            return g_api->make_number((double)g_rwlocks[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de rwlocks alcanzado");
}

static Result fn_rwlock_rdlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_rdlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    AcquireSRWLockShared(&r->lock);
#else
    pthread_rwlock_rdlock(&r->lock);
#endif
    return g_api->make_bool(1);
}

static Result fn_rwlock_wrlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_wrlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    AcquireSRWLockExclusive(&r->lock);
#else
    pthread_rwlock_wrlock(&r->lock);
#endif
    return g_api->make_bool(1);
}

static Result fn_rwlock_try_rdlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    int ok = 0;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_try_rdlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    ok = TryAcquireSRWLockShared(&r->lock) ? 1 : 0;
#else
    ok = (pthread_rwlock_tryrdlock(&r->lock) == 0) ? 1 : 0;
#endif
    return g_api->make_bool(ok);
}

static Result fn_rwlock_try_wrlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    int ok = 0;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_try_wrlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    ok = TryAcquireSRWLockExclusive(&r->lock) ? 1 : 0;
#else
    ok = (pthread_rwlock_trywrlock(&r->lock) == 0) ? 1 : 0;
#endif
    return g_api->make_bool(ok);
}

static Result fn_rwlock_rdunlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_rdunlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    ReleaseSRWLockShared(&r->lock);
#else
    pthread_rwlock_unlock(&r->lock);
#endif
    return g_api->make_bool(1);
}

static Result fn_rwlock_wrunlock(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_wrunlock(id)");
    lock_all();
    r = find_rwlock(id);
    unlock_all();
    if (!r) return fail(ERROR_NOMBRE, "rwlock id no existe");
#ifdef _WIN32
    ReleaseSRWLockExclusive(&r->lock);
#else
    pthread_rwlock_unlock(&r->lock);
#endif
    return g_api->make_bool(1);
}

static Result fn_rwlock_destroy(Result args[], int n_args) {
    int id;
    RwLockEntry* r;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.rwlock_destroy(id)");
    lock_all();
    r = find_rwlock(id);
    if (!r) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifndef _WIN32
    pthread_rwlock_destroy(&r->lock);
#elif defined(ITS_THREADS_LEGACY_WIN)
    DeleteCriticalSection(&r->lock.cs);
#endif
    memset(r, 0, sizeof(RwLockEntry));
    unlock_all();
    return g_api->make_bool(1);
}

/* -------------------- CondVar API -------------------- */

static CondEntry* find_cond(int id) {
    int i;
    for (i = 0; i < THR_MAX_CONDS; i++) if (g_conds[i].used && g_conds[i].id == id) return &g_conds[i];
    return NULL;
}

static Result fn_cond_new(Result args[], int n_args) {
    int i;
    (void)args;
    (void)n_args;
    lock_all();
    for (i = 0; i < THR_MAX_CONDS; i++) {
        if (!g_conds[i].used) {
            g_conds[i].used = 1;
            g_conds[i].id = g_next_cid++;
#ifdef _WIN32
            InitializeConditionVariable(&g_conds[i].cv);
#else
            pthread_cond_init(&g_conds[i].cv, NULL);
#endif
            unlock_all();
            return g_api->make_number((double)g_conds[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de condvars alcanzado");
}

static Result fn_cond_wait(Result args[], int n_args) {
    int cid;
    int mid;
    int timeout_ms = -1;
    CondEntry* c;
    MutexEntry* m;
    if (n_args < 2 || !parse_id(args[0], &cid) || !parse_id(args[1], &mid)) {
        return fail(ERROR_ARGUMENTO, "threads.cond_wait(cond_id, mutex_id, [timeout_ms])");
    }
    if (n_args >= 3 && is_number(args[2])) timeout_ms = (int)args[2].n;
    lock_all();
    c = find_cond(cid);
    m = find_mutex(mid);
    unlock_all();
    if (!c || !m) return fail(ERROR_NOMBRE, "cond o mutex no existe");
#ifdef _WIN32
    {
        DWORD t = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
        BOOL ok = SleepConditionVariableCS(&c->cv, &m->cs, t);
        if (ok) return g_api->make_bool(1);
        if (GetLastError() == ERROR_TIMEOUT) return g_api->make_bool(0);
        return fail(ERROR_SISTEMA, "SleepConditionVariableCS fallo");
    }
#else
    if (timeout_ms < 0) {
        if (pthread_cond_wait(&c->cv, &m->mtx) != 0) return fail(ERROR_SISTEMA, "pthread_cond_wait fallo");
        return g_api->make_bool(1);
    } else {
        struct timespec ts;
        int rc;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        rc = pthread_cond_timedwait(&c->cv, &m->mtx, &ts);
        if (rc == 0) return g_api->make_bool(1);
        if (rc == ETIMEDOUT) return g_api->make_bool(0);
        return fail(ERROR_SISTEMA, "pthread_cond_timedwait fallo");
    }
#endif
}

static Result fn_cond_signal(Result args[], int n_args) {
    int cid;
    CondEntry* c;
    if (n_args < 1 || !parse_id(args[0], &cid)) return fail(ERROR_ARGUMENTO, "threads.cond_signal(cond_id)");
    lock_all();
    c = find_cond(cid);
    unlock_all();
    if (!c) return fail(ERROR_NOMBRE, "cond id no existe");
#ifdef _WIN32
    WakeConditionVariable(&c->cv);
#else
    pthread_cond_signal(&c->cv);
#endif
    return g_api->make_bool(1);
}

static Result fn_cond_broadcast(Result args[], int n_args) {
    int cid;
    CondEntry* c;
    if (n_args < 1 || !parse_id(args[0], &cid)) return fail(ERROR_ARGUMENTO, "threads.cond_broadcast(cond_id)");
    lock_all();
    c = find_cond(cid);
    unlock_all();
    if (!c) return fail(ERROR_NOMBRE, "cond id no existe");
#ifdef _WIN32
    WakeAllConditionVariable(&c->cv);
#else
    pthread_cond_broadcast(&c->cv);
#endif
    return g_api->make_bool(1);
}

static Result fn_cond_destroy(Result args[], int n_args) {
    int cid;
    CondEntry* c;
    if (n_args < 1 || !parse_id(args[0], &cid)) return fail(ERROR_ARGUMENTO, "threads.cond_destroy(cond_id)");
    lock_all();
    c = find_cond(cid);
    if (!c) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifndef _WIN32
    pthread_cond_destroy(&c->cv);
#elif defined(ITS_THREADS_LEGACY_WIN)
    if (c->cv.event) CloseHandle(c->cv.event);
#endif
    memset(c, 0, sizeof(CondEntry));
    unlock_all();
    return g_api->make_bool(1);
}

/* -------------------- Semaphore API -------------------- */

static SemEntry* find_sem(int id) {
    int i;
    for (i = 0; i < THR_MAX_SEMS; i++) if (g_sems[i].used && g_sems[i].id == id) return &g_sems[i];
    return NULL;
}

static Result fn_sem_new(Result args[], int n_args) {
    int i;
    int initial;
    int maxv;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "threads.sem_new(initial, [max])");
    initial = (int)args[0].n;
    maxv = (n_args >= 2 && is_number(args[1])) ? (int)args[1].n : 32767;
    if (initial < 0) initial = 0;
    if (maxv < initial) maxv = initial;
    lock_all();
    for (i = 0; i < THR_MAX_SEMS; i++) {
        if (!g_sems[i].used) {
            g_sems[i].used = 1;
            g_sems[i].id = g_next_sid++;
#ifdef _WIN32
            g_sems[i].sem = CreateSemaphoreA(NULL, initial, maxv, NULL);
            if (!g_sems[i].sem) {
                memset(&g_sems[i], 0, sizeof(SemEntry));
                unlock_all();
                return fail(ERROR_SISTEMA, "CreateSemaphore fallo");
            }
            g_sems[i].value_estimate = initial;
#else
            if (sem_init(&g_sems[i].sem, 0, (unsigned int)initial) != 0) {
                memset(&g_sems[i], 0, sizeof(SemEntry));
                unlock_all();
                return fail(ERROR_SISTEMA, "sem_init fallo");
            }
#endif
            unlock_all();
            return g_api->make_number((double)g_sems[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de semaforos alcanzado");
}

static Result fn_sem_wait(Result args[], int n_args) {
    int sid;
    int timeout_ms = -1;
    SemEntry* s;
    if (n_args < 1 || !parse_id(args[0], &sid)) return fail(ERROR_ARGUMENTO, "threads.sem_wait(id, [timeout_ms])");
    if (n_args >= 2 && is_number(args[1])) timeout_ms = (int)args[1].n;
    lock_all();
    s = find_sem(sid);
    unlock_all();
    if (!s) return fail(ERROR_NOMBRE, "semaforo id no existe");
#ifdef _WIN32
    {
        DWORD wr = WaitForSingleObject(s->sem, (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms);
        if (wr == WAIT_OBJECT_0) {
            if (s->value_estimate > 0) s->value_estimate--;
            return g_api->make_bool(1);
        }
        if (wr == WAIT_TIMEOUT) return g_api->make_bool(0);
        return fail(ERROR_SISTEMA, "WaitForSingleObject sem fallo");
    }
#else
    if (timeout_ms < 0) {
        if (sem_wait(&s->sem) == 0) return g_api->make_bool(1);
        return fail(ERROR_SISTEMA, "sem_wait fallo");
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        if (sem_timedwait(&s->sem, &ts) == 0) return g_api->make_bool(1);
        if (errno == ETIMEDOUT) return g_api->make_bool(0);
        return fail(ERROR_SISTEMA, "sem_timedwait fallo");
    }
#endif
}

static Result fn_sem_try_wait(Result args[], int n_args) {
    int sid;
    SemEntry* s;
    if (n_args < 1 || !parse_id(args[0], &sid)) return fail(ERROR_ARGUMENTO, "threads.sem_try_wait(id)");
    lock_all();
    s = find_sem(sid);
    unlock_all();
    if (!s) return fail(ERROR_NOMBRE, "semaforo id no existe");
#ifdef _WIN32
    {
        DWORD wr = WaitForSingleObject(s->sem, 0);
        if (wr == WAIT_OBJECT_0) {
            if (s->value_estimate > 0) s->value_estimate--;
            return g_api->make_bool(1);
        }
        if (wr == WAIT_TIMEOUT) return g_api->make_bool(0);
        return fail(ERROR_SISTEMA, "sem_try_wait fallo");
    }
#else
    if (sem_trywait(&s->sem) == 0) return g_api->make_bool(1);
    if (errno == EAGAIN) return g_api->make_bool(0);
    return fail(ERROR_SISTEMA, "sem_trywait fallo");
#endif
}

static Result fn_sem_post(Result args[], int n_args) {
    int sid;
    SemEntry* s;
    if (n_args < 1 || !parse_id(args[0], &sid)) return fail(ERROR_ARGUMENTO, "threads.sem_post(id)");
    lock_all();
    s = find_sem(sid);
    unlock_all();
    if (!s) return fail(ERROR_NOMBRE, "semaforo id no existe");
#ifdef _WIN32
    if (!ReleaseSemaphore(s->sem, 1, NULL)) return fail(ERROR_SISTEMA, "ReleaseSemaphore fallo");
    s->value_estimate++;
#else
    if (sem_post(&s->sem) != 0) return fail(ERROR_SISTEMA, "sem_post fallo");
#endif
    return g_api->make_bool(1);
}

static Result fn_sem_value(Result args[], int n_args) {
    int sid;
    SemEntry* s;
    if (n_args < 1 || !parse_id(args[0], &sid)) return fail(ERROR_ARGUMENTO, "threads.sem_value(id)");
    lock_all();
    s = find_sem(sid);
    unlock_all();
    if (!s) return fail(ERROR_NOMBRE, "semaforo id no existe");
#ifdef _WIN32
    return g_api->make_number((double)s->value_estimate);
#else
    {
        int v = 0;
        sem_getvalue(&s->sem, &v);
        return g_api->make_number((double)v);
    }
#endif
}

static Result fn_sem_destroy(Result args[], int n_args) {
    int sid;
    SemEntry* s;
    if (n_args < 1 || !parse_id(args[0], &sid)) return fail(ERROR_ARGUMENTO, "threads.sem_destroy(id)");
    lock_all();
    s = find_sem(sid);
    if (!s) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifdef _WIN32
    CloseHandle(s->sem);
#else
    sem_destroy(&s->sem);
#endif
    memset(s, 0, sizeof(SemEntry));
    unlock_all();
    return g_api->make_bool(1);
}

/* -------------------- Barrier API -------------------- */

static BarrierEntry* find_barrier(int id) {
    int i;
    for (i = 0; i < THR_MAX_BARRIERS; i++) if (g_barriers[i].used && g_barriers[i].id == id) return &g_barriers[i];
    return NULL;
}

static Result fn_barrier_new(Result args[], int n_args) {
    int i;
    int parties;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "threads.barrier_new(parties)");
    parties = (int)args[0].n;
    if (parties < 1) return fail(ERROR_ARGUMENTO, "parties debe ser >= 1");
    lock_all();
    for (i = 0; i < THR_MAX_BARRIERS; i++) {
        if (!g_barriers[i].used) {
            g_barriers[i].used = 1;
            g_barriers[i].id = g_next_bid++;
            g_barriers[i].parties = parties;
            g_barriers[i].waiting = 0;
            g_barriers[i].generation = 0;
#ifdef _WIN32
            InitializeCriticalSection(&g_barriers[i].cs);
            InitializeConditionVariable(&g_barriers[i].cv);
#else
            pthread_mutex_init(&g_barriers[i].mtx, NULL);
            pthread_cond_init(&g_barriers[i].cv, NULL);
#endif
            unlock_all();
            return g_api->make_number((double)g_barriers[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de barriers alcanzado");
}

static Result fn_barrier_wait(Result args[], int n_args) {
    int id;
    BarrierEntry* b;
    int gen;
    int is_last = 0;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.barrier_wait(id)");
    lock_all();
    b = find_barrier(id);
    unlock_all();
    if (!b) return fail(ERROR_NOMBRE, "barrier id no existe");
#ifdef _WIN32
    EnterCriticalSection(&b->cs);
    gen = b->generation;
    b->waiting++;
    if (b->waiting >= b->parties) {
        b->waiting = 0;
        b->generation++;
        is_last = 1;
        WakeAllConditionVariable(&b->cv);
    } else {
        while (gen == b->generation) {
            SleepConditionVariableCS(&b->cv, &b->cs, INFINITE);
        }
    }
    LeaveCriticalSection(&b->cs);
#else
    pthread_mutex_lock(&b->mtx);
    gen = b->generation;
    b->waiting++;
    if (b->waiting >= b->parties) {
        b->waiting = 0;
        b->generation++;
        is_last = 1;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (gen == b->generation) {
            pthread_cond_wait(&b->cv, &b->mtx);
        }
    }
    pthread_mutex_unlock(&b->mtx);
#endif
    return g_api->make_bool(is_last ? 1 : 0);
}

static Result fn_barrier_destroy(Result args[], int n_args) {
    int id;
    BarrierEntry* b;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.barrier_destroy(id)");
    lock_all();
    b = find_barrier(id);
    if (!b) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifdef _WIN32
#ifdef ITS_THREADS_LEGACY_WIN
    if (b->cv.event) CloseHandle(b->cv.event);
#endif
    DeleteCriticalSection(&b->cs);
#else
    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->cv);
#endif
    memset(b, 0, sizeof(BarrierEntry));
    unlock_all();
    return g_api->make_bool(1);
}

/* -------------------- Atomic API -------------------- */

static AtomicEntry* find_atomic(int id) {
    int i;
    for (i = 0; i < THR_MAX_ATOMICS; i++) if (g_atomics[i].used && g_atomics[i].id == id) return &g_atomics[i];
    return NULL;
}

static void atomic_lock(AtomicEntry* a) {
#ifdef _WIN32
    EnterCriticalSection(&a->cs);
#else
    pthread_mutex_lock(&a->mtx);
#endif
}

static void atomic_unlock(AtomicEntry* a) {
#ifdef _WIN32
    LeaveCriticalSection(&a->cs);
#else
    pthread_mutex_unlock(&a->mtx);
#endif
}

static Result fn_atomic_new(Result args[], int n_args) {
    int i;
    long long initv = 0;
    if (n_args >= 1 && is_number(args[0])) initv = (long long)args[0].n;
    lock_all();
    for (i = 0; i < THR_MAX_ATOMICS; i++) {
        if (!g_atomics[i].used) {
            g_atomics[i].used = 1;
            g_atomics[i].id = g_next_aid++;
            g_atomics[i].value = initv;
#ifdef _WIN32
            InitializeCriticalSection(&g_atomics[i].cs);
#else
            pthread_mutex_init(&g_atomics[i].mtx, NULL);
#endif
            unlock_all();
            return g_api->make_number((double)g_atomics[i].id);
        }
    }
    unlock_all();
    return fail(ERROR_SISTEMA, "limite de atomics alcanzado");
}

static Result fn_atomic_get(Result args[], int n_args) {
    int id;
    AtomicEntry* a;
    long long v;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.atomic_get(id)");
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    v = a->value;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_set(Result args[], int n_args) {
    int id;
    AtomicEntry* a;
    long long v;
    if (n_args < 2 || !parse_id(args[0], &id) || !is_number(args[1])) {
        return fail(ERROR_ARGUMENTO, "threads.atomic_set(id, valor)");
    }
    v = (long long)args[1].n;
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    a->value = v;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_inc(Result args[], int n_args) {
    int id;
    AtomicEntry* a;
    long long v;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.atomic_inc(id)");
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    v = ++a->value;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_dec(Result args[], int n_args) {
    int id;
    AtomicEntry* a;
    long long v;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.atomic_dec(id)");
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    v = --a->value;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_add(Result args[], int n_args) {
    int id;
    long long addv;
    AtomicEntry* a;
    long long v;
    if (n_args < 2 || !parse_id(args[0], &id) || !is_number(args[1])) {
        return fail(ERROR_ARGUMENTO, "threads.atomic_add(id, delta)");
    }
    addv = (long long)args[1].n;
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    a->value += addv;
    v = a->value;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_sub(Result args[], int n_args) {
    int id;
    long long subv;
    AtomicEntry* a;
    long long v;
    if (n_args < 2 || !parse_id(args[0], &id) || !is_number(args[1])) {
        return fail(ERROR_ARGUMENTO, "threads.atomic_sub(id, delta)");
    }
    subv = (long long)args[1].n;
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    a->value -= subv;
    v = a->value;
    atomic_unlock(a);
    return g_api->make_number((double)v);
}

static Result fn_atomic_cas(Result args[], int n_args) {
    int id;
    long long expected;
    long long desired;
    AtomicEntry* a;
    int ok = 0;
    if (n_args < 3 || !parse_id(args[0], &id) || !is_number(args[1]) || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "threads.atomic_cas(id, expected, desired)");
    }
    expected = (long long)args[1].n;
    desired = (long long)args[2].n;
    lock_all();
    a = find_atomic(id);
    unlock_all();
    if (!a) return fail(ERROR_NOMBRE, "atomic id no existe");
    atomic_lock(a);
    if (a->value == expected) {
        a->value = desired;
        ok = 1;
    }
    atomic_unlock(a);
    return g_api->make_bool(ok);
}

static Result fn_atomic_destroy(Result args[], int n_args) {
    int id;
    AtomicEntry* a;
    if (n_args < 1 || !parse_id(args[0], &id)) return fail(ERROR_ARGUMENTO, "threads.atomic_destroy(id)");
    lock_all();
    a = find_atomic(id);
    if (!a) {
        unlock_all();
        return g_api->make_bool(0);
    }
#ifdef _WIN32
    DeleteCriticalSection(&a->cs);
#else
    pthread_mutex_destroy(&a->mtx);
#endif
    memset(a, 0, sizeof(AtomicEntry));
    unlock_all();
    return g_api->make_bool(1);
}

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;
    g_api = api;
    runtime_init();

    if (!api->export_const(module, "STATUS_NEW", api->make_number(THR_STATUS_NEW))) return 0;
    if (!api->export_const(module, "STATUS_RUNNING", api->make_number(THR_STATUS_RUNNING))) return 0;
    if (!api->export_const(module, "STATUS_DONE", api->make_number(THR_STATUS_DONE))) return 0;
    if (!api->export_const(module, "STATUS_FAILED", api->make_number(THR_STATUS_FAILED))) return 0;
    if (!api->export_const(module, "STATUS_CANCELLED", api->make_number(THR_STATUS_CANCELLED))) return 0;

    if (!api->export_native(module, "version", fn_version)) return 0;
    if (!api->export_native(module, "hardware_threads", fn_hw_threads)) return 0;
    if (!api->export_native(module, "now_ms", fn_now_ms)) return 0;
    if (!api->export_native(module, "sleep_ms", fn_sleep_ms)) return 0;
    if (!api->export_native(module, "yield_now", fn_yield_now)) return 0;

    if (!api->export_native(module, "spawn_sleep", fn_spawn_sleep)) return 0;
    if (!api->export_native(module, "spawn_system", fn_spawn_system)) return 0;
    if (!api->export_native(module, "spawn_process", fn_spawn_process)) return 0;
    if (!api->export_native(module, "spawn_itsuki", fn_spawn_itsuki)) return 0;
    if (!api->export_native(module, "spawn_sum", fn_spawn_sum)) return 0;
    if (!api->export_native(module, "spawn_fibonacci", fn_spawn_fib)) return 0;
    if (!api->export_native(module, "spawn_udp_sender", fn_spawn_udp_sender)) return 0;
    if (!api->export_native(module, "join", fn_join)) return 0;
    if (!api->export_native(module, "join_timeout", fn_join_timeout)) return 0;
    if (!api->export_native(module, "try_join", fn_try_join)) return 0;
    if (!api->export_native(module, "detach", fn_detach)) return 0;
    if (!api->export_native(module, "cancel", fn_cancel)) return 0;
    if (!api->export_native(module, "status", fn_status)) return 0;
    if (!api->export_native(module, "is_running", fn_is_running)) return 0;
    if (!api->export_native(module, "count_threads", fn_count_threads)) return 0;
    if (!api->export_native(module, "list_threads", fn_list_threads)) return 0;
    if (!api->export_native(module, "destroy", fn_destroy_thread)) return 0;
    if (!api->export_native(module, "cleanup", fn_cleanup_threads)) return 0;

    if (!api->export_native(module, "mutex_new", fn_mutex_new)) return 0;
    if (!api->export_native(module, "mutex_lock", fn_mutex_lock)) return 0;
    if (!api->export_native(module, "mutex_try_lock", fn_mutex_try_lock)) return 0;
    if (!api->export_native(module, "mutex_unlock", fn_mutex_unlock)) return 0;
    if (!api->export_native(module, "mutex_destroy", fn_mutex_destroy)) return 0;

    if (!api->export_native(module, "rwlock_new", fn_rwlock_new)) return 0;
    if (!api->export_native(module, "rwlock_rdlock", fn_rwlock_rdlock)) return 0;
    if (!api->export_native(module, "rwlock_wrlock", fn_rwlock_wrlock)) return 0;
    if (!api->export_native(module, "rwlock_try_rdlock", fn_rwlock_try_rdlock)) return 0;
    if (!api->export_native(module, "rwlock_try_wrlock", fn_rwlock_try_wrlock)) return 0;
    if (!api->export_native(module, "rwlock_rdunlock", fn_rwlock_rdunlock)) return 0;
    if (!api->export_native(module, "rwlock_wrunlock", fn_rwlock_wrunlock)) return 0;
    if (!api->export_native(module, "rwlock_destroy", fn_rwlock_destroy)) return 0;

    if (!api->export_native(module, "cond_new", fn_cond_new)) return 0;
    if (!api->export_native(module, "cond_wait", fn_cond_wait)) return 0;
    if (!api->export_native(module, "cond_signal", fn_cond_signal)) return 0;
    if (!api->export_native(module, "cond_broadcast", fn_cond_broadcast)) return 0;
    if (!api->export_native(module, "cond_destroy", fn_cond_destroy)) return 0;

    if (!api->export_native(module, "sem_new", fn_sem_new)) return 0;
    if (!api->export_native(module, "sem_wait", fn_sem_wait)) return 0;
    if (!api->export_native(module, "sem_try_wait", fn_sem_try_wait)) return 0;
    if (!api->export_native(module, "sem_post", fn_sem_post)) return 0;
    if (!api->export_native(module, "sem_value", fn_sem_value)) return 0;
    if (!api->export_native(module, "sem_destroy", fn_sem_destroy)) return 0;

    if (!api->export_native(module, "barrier_new", fn_barrier_new)) return 0;
    if (!api->export_native(module, "barrier_wait", fn_barrier_wait)) return 0;
    if (!api->export_native(module, "barrier_destroy", fn_barrier_destroy)) return 0;

    if (!api->export_native(module, "atomic_new", fn_atomic_new)) return 0;
    if (!api->export_native(module, "atomic_get", fn_atomic_get)) return 0;
    if (!api->export_native(module, "atomic_set", fn_atomic_set)) return 0;
    if (!api->export_native(module, "atomic_inc", fn_atomic_inc)) return 0;
    if (!api->export_native(module, "atomic_dec", fn_atomic_dec)) return 0;
    if (!api->export_native(module, "atomic_add", fn_atomic_add)) return 0;
    if (!api->export_native(module, "atomic_sub", fn_atomic_sub)) return 0;
    if (!api->export_native(module, "atomic_cas", fn_atomic_cas)) return 0;
    if (!api->export_native(module, "atomic_destroy", fn_atomic_destroy)) return 0;

    return 1;
}
