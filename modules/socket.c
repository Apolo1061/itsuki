#include "itsuki_ext.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
typedef SOCKET its_socket_t;
#define ITS_CLOSE_SOCKET closesocket
#else
typedef int its_socket_t;
#define ITS_CLOSE_SOCKET close
#endif

#define SOCK_MAX 4096

static const ItsukiApi* g_api = NULL;
static int g_last_error = 0;

#ifdef _WIN32
static LONG g_lock_state = 0;
static CRITICAL_SECTION g_lock;
static int g_wsa_started = 0;
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef struct {
    int used;
    int id;
    its_socket_t handle;
    int is_open;
    ObjSocket* obj;
} SocketEntry;

static SocketEntry g_socks[SOCK_MAX];
static int g_next_sid = 1;

static Result fail(TipoError tipo, const char* msg) {
    if (g_api && g_api->raise) g_api->raise(tipo, msg ? msg : "Error");
    return g_api ? g_api->make_null() : (Result){0};
}

static int is_number(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

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

static int socket_last_error_code(void) {
#ifdef _WIN32
    return (int)WSAGetLastError();
#else
    return errno;
#endif
}

static void remember_last_error(void) {
    g_last_error = socket_last_error_code();
}

static Result socket_throw(const char* what) {
    char msg[256];
    remember_last_error();
#ifdef _WIN32
    snprintf(msg, sizeof(msg), "%s (WSA: %d)", what, g_last_error);
#else
    snprintf(msg, sizeof(msg), "%s (errno: %d)", what, g_last_error);
#endif
    return fail(ERROR_EJECUCION, msg);
}

static int ensure_socket_runtime(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (g_wsa_started) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        remember_last_error();
        fail(ERROR_EJECUCION, "WSAStartup fallo");
        return 0;
    }
    g_wsa_started = 1;
#endif
    return 1;
}

static SocketEntry* find_entry_by_obj(ObjSocket* obj) {
    int i;
    if (!obj) return NULL;
    for (i = 0; i < SOCK_MAX; i++) {
        if (g_socks[i].used && g_socks[i].obj == obj) return &g_socks[i];
    }
    return NULL;
}

static SocketEntry* find_entry_by_id(int id) {
    int i;
    for (i = 0; i < SOCK_MAX; i++) {
        if (g_socks[i].used && g_socks[i].id == id) return &g_socks[i];
    }
    return NULL;
}

static SocketEntry* alloc_entry(void) {
    int i;
    for (i = 0; i < SOCK_MAX; i++) {
        if (!g_socks[i].used) {
            memset(&g_socks[i], 0, sizeof(SocketEntry));
            g_socks[i].used = 1;
            g_socks[i].id = g_next_sid++;
            return &g_socks[i];
        }
    }
    return NULL;
}

static ObjSocket* alloc_socket_obj(its_socket_t h) {
    ObjSocket* s = (ObjSocket*)malloc(sizeof(ObjSocket));
    if (!s) return NULL;
    memset(s, 0, sizeof(ObjSocket));
    s->obj.type = OBJ_SOCKET;
    s->obj.is_marked = false;
    s->obj.is_freed = false;
    s->obj.next = NULL;
    s->handle = (SocketHandle)h;
    s->is_open = true;
    return s;
}

static Result make_socket_result(its_socket_t h) {
    SocketEntry* e;
    ObjSocket* s;
    Result r = {0};
    e = alloc_entry();
    if (!e) return fail(ERROR_SISTEMA, "limite de sockets alcanzado");
    s = alloc_socket_obj(h);
    if (!s) {
        memset(e, 0, sizeof(SocketEntry));
        return fail(ERROR_SISTEMA, "sin memoria");
    }
    e->handle = h;
    e->is_open = 1;
    e->obj = s;

    r.tipo = TIPO_SOCKET;
    r.obj = (Obj*)s;
    return r;
}

typedef struct {
    its_socket_t handle;
    ObjSocket* obj;
    SocketEntry* entry;
} SockRef;

static int resolve_sock_arg(Result in, SockRef* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(SockRef));

    if (in.tipo == TIPO_SOCKET && in.obj) {
        ObjSocket* s = (ObjSocket*)in.obj;
        if (!s->is_open) {
            fail(ERROR_EJECUCION, "socket invalido o cerrado");
            return 0;
        }
        out->handle = (its_socket_t)s->handle;
        out->obj = s;
        lock_all();
        out->entry = find_entry_by_obj(s);
        unlock_all();
        return 1;
    }

    if (is_number(in)) {
        int id = (int)in.n;
        SocketEntry* e;
        lock_all();
        e = find_entry_by_id(id);
        if (!e || !e->is_open || !e->obj || !e->obj->is_open) {
            unlock_all();
            fail(ERROR_EJECUCION, "socket invalido o cerrado");
            return 0;
        }
        out->handle = e->handle;
        out->obj = e->obj;
        out->entry = e;
        unlock_all();
        return 1;
    }

    fail(ERROR_ARGUMENTO, "se esperaba socket");
    return 0;
}

static int close_sockref(SockRef* sref) {
    if (!sref || !sref->obj) return 0;
    if (!sref->obj->is_open) return 1;
    if (ITS_CLOSE_SOCKET((SOCKET)sref->handle) == SOCKET_ERROR) {
        remember_last_error();
        return 0;
    }
    sref->obj->is_open = false;
    if (sref->entry) {
        sref->entry->is_open = 0;
        sref->entry->handle = (its_socket_t)INVALID_SOCKET;
    }
    return 1;
}

static int fill_sockaddr(const char* host, int port, struct sockaddr_in* sa) {
    unsigned long ip;
    struct hostent* he;
    if (!host || !sa) return 0;
    memset(sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port = htons((unsigned short)port);

#ifdef _WIN32
    ip = inet_addr(host);
#else
    ip = (unsigned long)inet_addr(host);
#endif
    if (ip == INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        he = gethostbyname(host);
        if (!he || !he->h_addr_list || !he->h_addr_list[0]) return 0;
        memcpy(&sa->sin_addr, he->h_addr_list[0], he->h_length);
    } else {
        sa->sin_addr.s_addr = ip;
    }
    return 1;
}

static Result make_ip_port_array(const struct sockaddr_in* sa) {
    Result out = g_api->make_array();
    const char* ip = inet_ntoa(sa->sin_addr);
    g_api->array_push(out, g_api->make_string(ip ? ip : ""));
    g_api->array_push(out, g_api->make_number((double)ntohs(sa->sin_port)));
    return out;
}

static uint8_t* array_to_u8_buf(Array* arr, int* out_len) {
    int i;
    uint8_t* buf;
    if (!arr) {
        *out_len = 0;
        return NULL;
    }
    if (arr->tamano <= 0) {
        *out_len = 0;
        return NULL;
    }
    buf = (uint8_t*)malloc((size_t)arr->tamano);
    if (!buf) return NULL;
    for (i = 0; i < arr->tamano; i++) {
        Result v = arr->elementos[i];
        int n;
        if (!is_number(v)) {
            free(buf);
            return NULL;
        }
        n = (int)v.n;
        if (n < 0) n = 0;
        if (n > 255) n = 255;
        buf[i] = (uint8_t)n;
    }
    *out_len = arr->tamano;
    return buf;
}

static Result make_byte_array(const uint8_t* buf, int len) {
    int i;
    Result arr = g_api->make_array();
    for (i = 0; i < len; i++) {
        g_api->array_push(arr, g_api->make_number((double)buf[i]));
    }
    return arr;
}

static int bytes_find(const uint8_t* hay, int hay_len, const uint8_t* needle, int needle_len) {
    int i;
    int j;
    if (!hay || !needle || needle_len <= 0 || hay_len < needle_len) return -1;
    for (i = 0; i <= hay_len - needle_len; i++) {
        int ok = 1;
        for (j = 0; j < needle_len; j++) {
            if (hay[i + j] != needle[j]) {
                ok = 0;
                break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

static int str_find(const char* hay, int hay_len, const char* needle, int needle_len) {
    int i;
    if (!hay || !needle || needle_len <= 0 || hay_len < needle_len) return -1;
    for (i = 0; i <= hay_len - needle_len; i++) {
        if (memcmp(hay + i, needle, (size_t)needle_len) == 0) return i;
    }
    return -1;
}

static Result fn_socket(Result args[], int n_args) {
    int af = AF_INET;
    int type = SOCK_STREAM;
    int proto = 0;
    SOCKET h;

    if (!ensure_socket_runtime()) return g_api->make_null();

    if (n_args > 0 && is_number(args[0])) af = (int)args[0].n;
    if (n_args > 1 && is_number(args[1])) type = (int)args[1].n;
    if (n_args > 2 && is_number(args[2])) proto = (int)args[2].n;

#ifdef _WIN32
    if (type == SOCK_RAW) return fail(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#endif

    h = socket(af, type, proto);
    if (h == INVALID_SOCKET) return socket_throw("Fallo socket");
    return make_socket_result((its_socket_t)h);
}

static Result fn_raw_socket(Result args[], int n_args) {
#ifdef _WIN32
    (void)args;
    (void)n_args;
    return fail(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#else
    int proto = IPPROTO_RAW;
    int af = AF_INET;
    if (!ensure_socket_runtime()) return g_api->make_null();
    if (n_args > 0 && is_number(args[0])) proto = (int)args[0].n;
    if (n_args > 1 && is_number(args[1])) af = (int)args[1].n;
    return fn_socket((Result[]){g_api->make_number((double)af), g_api->make_number((double)SOCK_RAW), g_api->make_number((double)proto)}, 3);
#endif
}

static Result fn_connect(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    if (n_args < 3 || args[1].tipo != TIPO_CADENA || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "socket.connect(sock, host, port)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!fill_sockaddr(args[1].s, (int)args[2].n, &sa)) {
        return fail(ERROR_NOMBRE, "No se pudo resolver host");
    }
    if (connect((SOCKET)sref.handle, (struct sockaddr*)&sa, (socklen_t)sizeof(sa)) == SOCKET_ERROR) {
        return socket_throw("Error conexion TCP");
    }
    return g_api->make_number(0);
}

static Result fn_send(Result args[], int n_args) {
    SockRef sref;
    int sent;
    if (n_args < 2 || args[1].tipo != TIPO_CADENA) return fail(ERROR_ARGUMENTO, "socket.send(sock, data)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    sent = (int)send((SOCKET)sref.handle, args[1].s ? args[1].s : "", (int)strlen(args[1].s ? args[1].s : ""), 0);
    if (sent == SOCKET_ERROR) return socket_throw("Fallo send");
    return g_api->make_number((double)sent);
}

static Result fn_send_bytes(Result args[], int n_args) {
    SockRef sref;
    uint8_t* buf;
    int len = 0;
    int sent;
    if (n_args < 2 || args[1].tipo != TIPO_ARRAY || !args[1].a) return fail(ERROR_ARGUMENTO, "socket.send_bytes(sock, bytes)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    buf = array_to_u8_buf(args[1].a, &len);
    if (args[1].a->tamano > 0 && !buf) return fail(ERROR_TIPO, "bytes invalidos");
    if (len <= 0) {
        free(buf);
        return g_api->make_number(0);
    }
    sent = (int)send((SOCKET)sref.handle, (const char*)buf, len, 0);
    free(buf);
    if (sent == SOCKET_ERROR) return socket_throw("Fallo send_bytes");
    return g_api->make_number((double)sent);
}

static Result fn_send_all(Result args[], int n_args) {
    SockRef sref;
    if (n_args < 2) return fail(ERROR_ARGUMENTO, "socket.send_all(sock, data)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();

    if (args[1].tipo == TIPO_ARRAY && args[1].a) {
        uint8_t* buf;
        int len = 0;
        int off = 0;
        buf = array_to_u8_buf(args[1].a, &len);
        if (args[1].a->tamano > 0 && !buf) return fail(ERROR_TIPO, "bytes invalidos");
        while (off < len) {
            int sent = (int)send((SOCKET)sref.handle, (const char*)(buf + off), len - off, 0);
            if (sent == SOCKET_ERROR) {
                free(buf);
                return socket_throw("Fallo send_all");
            }
            if (sent <= 0) {
                free(buf);
                return fail(ERROR_EJECUCION, "send_all: conexion cerrada");
            }
            off += sent;
        }
        free(buf);
        return g_api->make_number((double)len);
    }

    if (args[1].tipo == TIPO_CADENA) {
        const char* s = args[1].s ? args[1].s : "";
        int n = (int)strlen(s);
        int off = 0;
        while (off < n) {
            int sent = (int)send((SOCKET)sref.handle, s + off, n - off, 0);
            if (sent == SOCKET_ERROR) return socket_throw("Fallo send_all");
            if (sent <= 0) return fail(ERROR_EJECUCION, "send_all: conexion cerrada");
            off += sent;
        }
        return g_api->make_number((double)n);
    }

    return fail(ERROR_ARGUMENTO, "socket.send_all(sock, data)");
}

static Result fn_recv(Result args[], int n_args) {
    SockRef sref;
    int buflen = 4096;
    char* buf;
    int got;
    Result out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.recv(sock, [buflen])");
    if (n_args > 1 && is_number(args[1])) buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();

    buf = (char*)malloc((size_t)buflen + 1);
    if (!buf) return fail(ERROR_SISTEMA, "sin memoria");
    got = (buflen > 0) ? (int)recv((SOCKET)sref.handle, buf, buflen, 0) : 0;
    if (got == SOCKET_ERROR) {
        free(buf);
        return socket_throw("Fallo recv");
    }
    buf[got] = '\0';
    out = g_api->make_string(buf);
    free(buf);
    return out;
}

static Result fn_recv_bytes(Result args[], int n_args) {
    SockRef sref;
    int buflen = 4096;
    uint8_t* buf;
    int got;
    Result out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.recv_bytes(sock, [buflen])");
    if (n_args > 1 && is_number(args[1])) buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();

    buf = (buflen > 0) ? (uint8_t*)malloc((size_t)buflen) : NULL;
    got = (buflen > 0) ? (int)recv((SOCKET)sref.handle, (char*)buf, buflen, 0) : 0;
    if (got == SOCKET_ERROR) {
        free(buf);
        return socket_throw("Fallo recv_bytes");
    }
    out = make_byte_array(buf, got);
    free(buf);
    return out;
}

static Result fn_recv_exact(Result args[], int n_args) {
    SockRef sref;
    int want;
    uint8_t* buf;
    int off = 0;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.recv_exact(sock, n)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    want = (int)args[1].n;
    if (want < 0) want = 0;

    buf = (want > 0) ? (uint8_t*)malloc((size_t)want) : NULL;
    while (off < want) {
        int got = (int)recv((SOCKET)sref.handle, (char*)(buf + off), want - off, 0);
        if (got == SOCKET_ERROR) {
            free(buf);
            return socket_throw("Fallo recv_exact");
        }
        if (got == 0) {
            free(buf);
            return fail(ERROR_EJECUCION, "recv_exact: EOF");
        }
        off += got;
    }
    {
        Result out = make_byte_array(buf, want);
        free(buf);
        return out;
    }
}

static Result fn_recv_until(Result args[], int n_args) {
    SockRef sref;
    int max_len = 1024 * 1024;

    if (n_args < 2) return fail(ERROR_ARGUMENTO, "socket.recv_until(sock, delim, [max_len])");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (n_args > 2 && is_number(args[2])) max_len = (int)args[2].n;
    if (max_len <= 0) max_len = 1024 * 1024;

    if (args[1].tipo == TIPO_ARRAY && args[1].a) {
        uint8_t* delim;
        int delim_len = 0;
        uint8_t* buf = NULL;
        int buf_len = 0;
        delim = array_to_u8_buf(args[1].a, &delim_len);
        if (!delim || delim_len <= 0) {
            free(delim);
            return fail(ERROR_ARGUMENTO, "delim bytes invalido");
        }
        while (buf_len <= max_len) {
            uint8_t tmp[4096];
            int got = (int)recv((SOCKET)sref.handle, (char*)tmp, (int)sizeof(tmp), 0);
            int ix;
            uint8_t* nb;
            if (got == SOCKET_ERROR) {
                free(delim);
                free(buf);
                return socket_throw("Fallo recv_until");
            }
            if (got == 0) {
                free(delim);
                free(buf);
                return fail(ERROR_EJECUCION, "recv_until: EOF");
            }
            nb = (uint8_t*)realloc(buf, (size_t)(buf_len + got));
            if (!nb) {
                free(delim);
                free(buf);
                return fail(ERROR_SISTEMA, "sin memoria");
            }
            buf = nb;
            memcpy(buf + buf_len, tmp, (size_t)got);
            buf_len += got;
            ix = bytes_find(buf, buf_len, delim, delim_len);
            if (ix != -1) {
                Result out = make_byte_array(buf, ix + delim_len);
                free(delim);
                free(buf);
                return out;
            }
        }
        free(delim);
        free(buf);
        return fail(ERROR_EJECUCION, "recv_until: excedio max_len");
    }

    if (args[1].tipo == TIPO_CADENA && args[1].s) {
        const char* delim = args[1].s;
        int delim_len = (int)strlen(delim);
        char* buf = NULL;
        int buf_len = 0;
        if (delim_len <= 0) return fail(ERROR_ARGUMENTO, "delim vacio");
        while (buf_len <= max_len) {
            char tmp[4096];
            int got = (int)recv((SOCKET)sref.handle, tmp, (int)sizeof(tmp), 0);
            int ix;
            char* nb;
            if (got == SOCKET_ERROR) {
                free(buf);
                return socket_throw("Fallo recv_until");
            }
            if (got == 0) {
                free(buf);
                return fail(ERROR_EJECUCION, "recv_until: EOF");
            }
            nb = (char*)realloc(buf, (size_t)(buf_len + got + 1));
            if (!nb) {
                free(buf);
                return fail(ERROR_SISTEMA, "sin memoria");
            }
            buf = nb;
            memcpy(buf + buf_len, tmp, (size_t)got);
            buf_len += got;
            buf[buf_len] = '\0';
            ix = str_find(buf, buf_len, delim, delim_len);
            if (ix != -1) {
                int out_len = ix + delim_len;
                char* out = (char*)malloc((size_t)out_len + 1);
                Result r;
                if (!out) {
                    free(buf);
                    return fail(ERROR_SISTEMA, "sin memoria");
                }
                memcpy(out, buf, (size_t)out_len);
                out[out_len] = '\0';
                r = g_api->make_string(out);
                free(out);
                free(buf);
                return r;
            }
        }
        free(buf);
        return fail(ERROR_EJECUCION, "recv_until: excedio max_len");
    }

    return fail(ERROR_ARGUMENTO, "socket.recv_until(sock, delim, [max_len])");
}

static Result fn_close(Result args[], int n_args) {
    SockRef sref;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.close(sock)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!close_sockref(&sref)) return socket_throw("Fallo close");
    return g_api->make_number(0);
}

static Result fn_bind(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    if (n_args < 3 || args[1].tipo != TIPO_CADENA || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "socket.bind(sock, host, port)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!fill_sockaddr(args[1].s, (int)args[2].n, &sa)) return fail(ERROR_NOMBRE, "No se pudo resolver host");
    if (bind((SOCKET)sref.handle, (struct sockaddr*)&sa, (socklen_t)sizeof(sa)) == SOCKET_ERROR) {
        return socket_throw("Fallo bind");
    }
    return g_api->make_number(0);
}

static Result fn_listen(Result args[], int n_args) {
    SockRef sref;
    int backlog = 5;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.listen(sock, [backlog])");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (n_args > 1 && is_number(args[1])) backlog = (int)args[1].n;
    if (listen((SOCKET)sref.handle, backlog) == SOCKET_ERROR) return socket_throw("Fallo listen");
    return g_api->make_number(0);
}

static Result fn_accept(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    socklen_t sa_len = (socklen_t)sizeof(sa);
    SOCKET ch;
    Result out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.accept(sock)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    memset(&sa, 0, sizeof(sa));
    ch = accept((SOCKET)sref.handle, (struct sockaddr*)&sa, &sa_len);
    if (ch == INVALID_SOCKET) return socket_throw("Fallo accept");

    out = g_api->make_array();
    g_api->array_push(out, make_socket_result((its_socket_t)ch));
    g_api->array_push(out, g_api->make_string(inet_ntoa(sa.sin_addr)));
    g_api->array_push(out, g_api->make_number((double)ntohs(sa.sin_port)));
    return out;
}

static Result fn_sendto(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    int sent;
    if (n_args < 4 || args[1].tipo != TIPO_CADENA || args[2].tipo != TIPO_CADENA || !is_number(args[3])) {
        return fail(ERROR_ARGUMENTO, "socket.sendto(sock, data, host, port)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!fill_sockaddr(args[2].s, (int)args[3].n, &sa)) return fail(ERROR_NOMBRE, "No se pudo resolver host");
    sent = (int)sendto((SOCKET)sref.handle, args[1].s ? args[1].s : "", (int)strlen(args[1].s ? args[1].s : ""), 0, (struct sockaddr*)&sa, (socklen_t)sizeof(sa));
    if (sent == SOCKET_ERROR) return socket_throw("Fallo sendto");
    return g_api->make_number((double)sent);
}

static Result fn_sendto_bytes(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    uint8_t* buf;
    int len = 0;
    int sent;
    if (n_args < 4 || args[1].tipo != TIPO_ARRAY || !args[1].a || args[2].tipo != TIPO_CADENA || !is_number(args[3])) {
        return fail(ERROR_ARGUMENTO, "socket.sendto_bytes(sock, bytes, host, port)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!fill_sockaddr(args[2].s, (int)args[3].n, &sa)) return fail(ERROR_NOMBRE, "No se pudo resolver host");
    buf = array_to_u8_buf(args[1].a, &len);
    if (args[1].a->tamano > 0 && !buf) return fail(ERROR_TIPO, "bytes invalidos");
    sent = (len > 0)
        ? (int)sendto((SOCKET)sref.handle, (const char*)buf, len, 0, (struct sockaddr*)&sa, (socklen_t)sizeof(sa))
        : 0;
    free(buf);
    if (sent == SOCKET_ERROR) return socket_throw("Fallo sendto_bytes");
    return g_api->make_number((double)sent);
}

static Result fn_set_hdrincl(Result args[], int n_args) {
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_hdrincl(sock, enabled)");
#ifdef _WIN32
    (void)args;
    return fail(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#else
    SockRef sref;
    int enabled;
    int opt;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    enabled = ((int)args[1].n != 0);
    opt = enabled ? 1 : 0;
    if (setsockopt((SOCKET)sref.handle, IPPROTO_IP, IP_HDRINCL, (char*)&opt, (socklen_t)sizeof(opt)) == SOCKET_ERROR) {
        return socket_throw("Fallo set_hdrincl");
    }
    return g_api->make_number(0);
#endif
}

static Result fn_sendto_raw(Result args[], int n_args) {
    if (n_args < 4 || args[1].tipo != TIPO_ARRAY || !args[1].a || args[2].tipo != TIPO_CADENA || !is_number(args[3])) {
        return fail(ERROR_ARGUMENTO, "socket.sendto_raw(sock, bytes, host, port)");
    }
#ifdef _WIN32
    (void)args;
    return fail(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#else
    SockRef sref;
    struct sockaddr_in sa;
    uint8_t* buf;
    int len = 0;
    int sent;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    if (!fill_sockaddr(args[2].s, (int)args[3].n, &sa)) return fail(ERROR_NOMBRE, "No se pudo resolver host");
    buf = array_to_u8_buf(args[1].a, &len);
    if (args[1].a->tamano > 0 && !buf) return fail(ERROR_TIPO, "bytes invalidos");
    sent = (len > 0)
        ? (int)sendto((SOCKET)sref.handle, (const char*)buf, len, 0, (struct sockaddr*)&sa, (socklen_t)sizeof(sa))
        : 0;
    free(buf);
    if (sent == SOCKET_ERROR) return socket_throw("Fallo sendto_raw");
    return g_api->make_number((double)sent);
#endif
}

static Result fn_setsockopt(Result args[], int n_args) {
    SockRef sref;
    int level;
    int opt;
    int value;
    if (n_args < 4 || !is_number(args[1]) || !is_number(args[2]) || !is_number(args[3])) {
        return fail(ERROR_ARGUMENTO, "socket.setsockopt(sock, level, opt, value)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    level = (int)args[1].n;
    opt = (int)args[2].n;
    value = (int)args[3].n;
    if (setsockopt((SOCKET)sref.handle, level, opt, (char*)&value, (socklen_t)sizeof(value)) == SOCKET_ERROR) {
        return socket_throw("Fallo setsockopt");
    }
    return g_api->make_number(0);
}

static Result fn_getsockopt(Result args[], int n_args) {
    SockRef sref;
    int level;
    int opt;
    int value = 0;
    socklen_t len = (socklen_t)sizeof(value);
    if (n_args < 3 || !is_number(args[1]) || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "socket.getsockopt(sock, level, opt)");
    }
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    level = (int)args[1].n;
    opt = (int)args[2].n;
    if (getsockopt((SOCKET)sref.handle, level, opt, (char*)&value, &len) == SOCKET_ERROR) {
        return socket_throw("Fallo getsockopt");
    }
    return g_api->make_number((double)value);
}

static Result fn_set_timeout(Result args[], int n_args) {
    SockRef sref;
    int recv_ms;
    int send_ms;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_timeout(sock, recv_ms, [send_ms])");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    recv_ms = (int)args[1].n;
    send_ms = (n_args > 2 && is_number(args[2])) ? (int)args[2].n : recv_ms;
#ifdef _WIN32
    {
        DWORD r = (DWORD)recv_ms;
        DWORD s = (DWORD)send_ms;
        if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&r, (socklen_t)sizeof(r)) == SOCKET_ERROR) {
            return socket_throw("Fallo set_timeout recv");
        }
        if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_SNDTIMEO, (char*)&s, (socklen_t)sizeof(s)) == SOCKET_ERROR) {
            return socket_throw("Fallo set_timeout send");
        }
    }
#else
    {
        struct timeval tr;
        struct timeval ts;
        tr.tv_sec = recv_ms / 1000;
        tr.tv_usec = (recv_ms % 1000) * 1000;
        ts.tv_sec = send_ms / 1000;
        ts.tv_usec = (send_ms % 1000) * 1000;
        if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_RCVTIMEO, &tr, (socklen_t)sizeof(tr)) == SOCKET_ERROR) {
            return socket_throw("Fallo set_timeout recv");
        }
        if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_SNDTIMEO, &ts, (socklen_t)sizeof(ts)) == SOCKET_ERROR) {
            return socket_throw("Fallo set_timeout send");
        }
    }
#endif
    return g_api->make_number(0);
}

static Result fn_shutdown(Result args[], int n_args) {
    SockRef sref;
    int how;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.shutdown(sock, how)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    how = (int)args[1].n;
    if (shutdown((SOCKET)sref.handle, how) == SOCKET_ERROR) return socket_throw("Fallo shutdown");
    return g_api->make_number(0);
}

static Result fn_recvfrom(Result args[], int n_args) {
    SockRef sref;
    int buflen = 4096;
    char* buf;
    struct sockaddr_in sa;
    socklen_t sa_len = (socklen_t)sizeof(sa);
    int got;
    Result out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.recvfrom(sock, [buflen])");
    if (n_args > 1 && is_number(args[1])) buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();

    buf = (char*)malloc((size_t)buflen + 1);
    if (!buf) return fail(ERROR_SISTEMA, "sin memoria");
    memset(&sa, 0, sizeof(sa));
    got = (buflen > 0)
        ? (int)recvfrom((SOCKET)sref.handle, buf, buflen, 0, (struct sockaddr*)&sa, &sa_len)
        : 0;
    if (got == SOCKET_ERROR) {
        free(buf);
        return socket_throw("Fallo recvfrom");
    }
    buf[got] = '\0';

    out = g_api->make_array();
    g_api->array_push(out, g_api->make_string(buf));
    g_api->array_push(out, g_api->make_string(inet_ntoa(sa.sin_addr)));
    g_api->array_push(out, g_api->make_number((double)ntohs(sa.sin_port)));
    free(buf);
    return out;
}

static Result fn_recvfrom_bytes(Result args[], int n_args) {
    SockRef sref;
    int buflen = 4096;
    uint8_t* buf;
    struct sockaddr_in sa;
    socklen_t sa_len = (socklen_t)sizeof(sa);
    int got;
    Result out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.recvfrom_bytes(sock, [buflen])");
    if (n_args > 1 && is_number(args[1])) buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();

    buf = (buflen > 0) ? (uint8_t*)malloc((size_t)buflen) : NULL;
    memset(&sa, 0, sizeof(sa));
    got = (buflen > 0)
        ? (int)recvfrom((SOCKET)sref.handle, (char*)buf, buflen, 0, (struct sockaddr*)&sa, &sa_len)
        : 0;
    if (got == SOCKET_ERROR) {
        free(buf);
        return socket_throw("Fallo recvfrom_bytes");
    }

    out = g_api->make_array();
    g_api->array_push(out, make_byte_array(buf, got));
    g_api->array_push(out, g_api->make_string(inet_ntoa(sa.sin_addr)));
    g_api->array_push(out, g_api->make_number((double)ntohs(sa.sin_port)));
    free(buf);
    return out;
}

static Result fn_set_nonblocking(Result args[], int n_args) {
    SockRef sref;
    int enabled;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_nonblocking(sock, enabled)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    enabled = ((int)args[1].n != 0);
#ifdef _WIN32
    {
        u_long mode = enabled ? 1UL : 0UL;
        if (ioctlsocket((SOCKET)sref.handle, FIONBIO, &mode) != 0) return socket_throw("Fallo set_nonblocking");
    }
#else
    {
        int fd = (int)sref.handle;
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) return socket_throw("Fallo set_nonblocking");
        if (enabled) flags |= O_NONBLOCK;
        else flags &= ~O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0) return socket_throw("Fallo set_nonblocking");
    }
#endif
    return g_api->make_number(0);
}

static int add_fd_from_result(Result r, fd_set* fds
#ifndef _WIN32
, int* maxfd
#endif
) {
    SockRef sref;
    if (r.tipo != TIPO_SOCKET && !is_number(r)) return 0;
    if (!resolve_sock_arg(r, &sref)) return 0;
    FD_SET((SOCKET)sref.handle, fds);
#ifndef _WIN32
    if ((int)sref.handle > *maxfd) *maxfd = (int)sref.handle;
#endif
    return 1;
}

static Result fn_select(Result args[], int n_args) {
    Array* read_in = NULL;
    Array* write_in = NULL;
    int timeout_ms = -1;
    fd_set rfds;
    fd_set wfds;
    struct timeval tv;
    struct timeval* ptv = NULL;
    int rc;
    Result read_ready;
    Result write_ready;
    Result out;
    int i;
#ifndef _WIN32
    int maxfd = 0;
#endif

    if (n_args > 0 && args[0].tipo == TIPO_ARRAY) read_in = args[0].a;
    if (n_args > 1 && args[1].tipo == TIPO_ARRAY) write_in = args[1].a;
    if (n_args > 2 && is_number(args[2])) timeout_ms = (int)args[2].n;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (read_in) {
        for (i = 0; i < read_in->tamano; i++) {
            add_fd_from_result(read_in->elementos[i], &rfds
#ifndef _WIN32
            , &maxfd
#endif
            );
        }
    }

    if (write_in) {
        for (i = 0; i < write_in->tamano; i++) {
            add_fd_from_result(write_in->elementos[i], &wfds
#ifndef _WIN32
            , &maxfd
#endif
            );
        }
    }

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

#ifdef _WIN32
    rc = (int)select(0, &rfds, &wfds, NULL, ptv);
#else
    rc = (int)select(maxfd + 1, &rfds, &wfds, NULL, ptv);
#endif
    if (rc == SOCKET_ERROR) return socket_throw("Fallo select");

    read_ready = g_api->make_array();
    write_ready = g_api->make_array();

    if (read_in) {
        for (i = 0; i < read_in->tamano; i++) {
            SockRef sref;
            Result r = read_in->elementos[i];
            if ((r.tipo != TIPO_SOCKET && !is_number(r)) || !resolve_sock_arg(r, &sref)) continue;
            if (FD_ISSET((SOCKET)sref.handle, &rfds)) g_api->array_push(read_ready, r);
        }
    }
    if (write_in) {
        for (i = 0; i < write_in->tamano; i++) {
            SockRef sref;
            Result r = write_in->elementos[i];
            if ((r.tipo != TIPO_SOCKET && !is_number(r)) || !resolve_sock_arg(r, &sref)) continue;
            if (FD_ISSET((SOCKET)sref.handle, &wfds)) g_api->array_push(write_ready, r);
        }
    }

    out = g_api->make_array();
    g_api->array_push(out, read_ready);
    g_api->array_push(out, write_ready);
    return out;
}

static Result fn_getsockname(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    socklen_t len = (socklen_t)sizeof(sa);
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.getsockname(sock)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    memset(&sa, 0, sizeof(sa));
    if (getsockname((SOCKET)sref.handle, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) return socket_throw("Fallo getsockname");
    return make_ip_port_array(&sa);
}

static Result fn_getpeername(Result args[], int n_args) {
    SockRef sref;
    struct sockaddr_in sa;
    socklen_t len = (socklen_t)sizeof(sa);
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "socket.getpeername(sock)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    memset(&sa, 0, sizeof(sa));
    if (getpeername((SOCKET)sref.handle, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) return socket_throw("Fallo getpeername");
    return make_ip_port_array(&sa);
}

static Result fn_set_reuseaddr(Result args[], int n_args) {
    SockRef sref;
    int opt;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_reuseaddr(sock, enabled)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    opt = ((int)args[1].n != 0) ? 1 : 0;
    if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, (socklen_t)sizeof(opt)) == SOCKET_ERROR) {
        return socket_throw("Fallo set_reuseaddr");
    }
    return g_api->make_number(0);
}

static Result fn_set_keepalive(Result args[], int n_args) {
    SockRef sref;
    int opt;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_keepalive(sock, enabled)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    opt = ((int)args[1].n != 0) ? 1 : 0;
    if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, (socklen_t)sizeof(opt)) == SOCKET_ERROR) {
        return socket_throw("Fallo set_keepalive");
    }
    return g_api->make_number(0);
}

static Result fn_set_nodelay(Result args[], int n_args) {
    SockRef sref;
    int opt;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_nodelay(sock, enabled)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    opt = ((int)args[1].n != 0) ? 1 : 0;
    if (setsockopt((SOCKET)sref.handle, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, (socklen_t)sizeof(opt)) == SOCKET_ERROR) {
        return socket_throw("Fallo set_nodelay");
    }
    return g_api->make_number(0);
}

static Result fn_set_broadcast(Result args[], int n_args) {
    SockRef sref;
    int opt;
    if (n_args < 2 || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "socket.set_broadcast(sock, enabled)");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    opt = ((int)args[1].n != 0) ? 1 : 0;
    if (setsockopt((SOCKET)sref.handle, SOL_SOCKET, SO_BROADCAST, (char*)&opt, (socklen_t)sizeof(opt)) == SOCKET_ERROR) {
        return socket_throw("Fallo set_broadcast");
    }
    return g_api->make_number(0);
}

static Result fn_multicast_join(Result args[], int n_args) {
    SockRef sref;
    struct ip_mreq mreq;
    const char* group;
    const char* iface = "0.0.0.0";
    if (n_args < 2 || args[1].tipo != TIPO_CADENA) return fail(ERROR_ARGUMENTO, "socket.multicast_join(sock, group, [iface])");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    group = args[1].s;
    if (n_args > 2 && args[2].tipo == TIPO_CADENA && args[2].s) iface = args[2].s;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = inet_addr(iface);
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE) return fail(ERROR_ARGUMENTO, "Grupo multicast invalido");
    if (setsockopt((SOCKET)sref.handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, (socklen_t)sizeof(mreq)) == SOCKET_ERROR) {
        return socket_throw("Fallo multicast_join");
    }
    return g_api->make_number(0);
}

static Result fn_multicast_leave(Result args[], int n_args) {
    SockRef sref;
    struct ip_mreq mreq;
    const char* group;
    const char* iface = "0.0.0.0";
    if (n_args < 2 || args[1].tipo != TIPO_CADENA) return fail(ERROR_ARGUMENTO, "socket.multicast_leave(sock, group, [iface])");
    if (!resolve_sock_arg(args[0], &sref)) return g_api->make_null();
    group = args[1].s;
    if (n_args > 2 && args[2].tipo == TIPO_CADENA && args[2].s) iface = args[2].s;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = inet_addr(iface);
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE) return fail(ERROR_ARGUMENTO, "Grupo multicast invalido");
    if (setsockopt((SOCKET)sref.handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, (socklen_t)sizeof(mreq)) == SOCKET_ERROR) {
        return socket_throw("Fallo multicast_leave");
    }
    return g_api->make_number(0);
}

static Result fn_last_error(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    if (g_last_error == 0) remember_last_error();
    return g_api->make_number((double)g_last_error);
}

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;
    g_api = api;

    if (!ensure_socket_runtime()) return 0;

    if (!api->export_const(module, "AF_INET", api->make_number(2))) return 0;
    if (!api->export_const(module, "SOCK_STREAM", api->make_number(1))) return 0;
    if (!api->export_const(module, "SOCK_DGRAM", api->make_number(2))) return 0;
    if (!api->export_const(module, "SOCK_RAW", api->make_number(3))) return 0;
    if (!api->export_const(module, "IPPROTO_ICMP", api->make_number(1))) return 0;
    if (!api->export_const(module, "IPPROTO_TCP", api->make_number(6))) return 0;
    if (!api->export_const(module, "IPPROTO_UDP", api->make_number(17))) return 0;
    if (!api->export_const(module, "IPPROTO_RAW", api->make_number(255))) return 0;
    if (!api->export_const(module, "SOL_SOCKET", api->make_number(1))) return 0;
    if (!api->export_const(module, "SO_REUSEADDR", api->make_number(2))) return 0;
    if (!api->export_const(module, "SO_BROADCAST", api->make_number(6))) return 0;
    if (!api->export_const(module, "SO_RCVTIMEO", api->make_number(20))) return 0;
    if (!api->export_const(module, "SO_SNDTIMEO", api->make_number(21))) return 0;
    if (!api->export_const(module, "TCP_NODELAY", api->make_number(1))) return 0;
    if (!api->export_const(module, "SHUT_RD", api->make_number(0))) return 0;
    if (!api->export_const(module, "SHUT_WR", api->make_number(1))) return 0;
    if (!api->export_const(module, "SHUT_RDWR", api->make_number(2))) return 0;

    if (!api->export_native(module, "socket", fn_socket)) return 0;
    if (!api->export_native(module, "raw_socket", fn_raw_socket)) return 0;
    if (!api->export_native(module, "connect", fn_connect)) return 0;
    if (!api->export_native(module, "send", fn_send)) return 0;
    if (!api->export_native(module, "send_bytes", fn_send_bytes)) return 0;
    if (!api->export_native(module, "recv", fn_recv)) return 0;
    if (!api->export_native(module, "recv_bytes", fn_recv_bytes)) return 0;
    if (!api->export_native(module, "close", fn_close)) return 0;
    if (!api->export_native(module, "bind", fn_bind)) return 0;
    if (!api->export_native(module, "listen", fn_listen)) return 0;
    if (!api->export_native(module, "accept", fn_accept)) return 0;
    if (!api->export_native(module, "sendto", fn_sendto)) return 0;
    if (!api->export_native(module, "sendto_bytes", fn_sendto_bytes)) return 0;
    if (!api->export_native(module, "set_hdrincl", fn_set_hdrincl)) return 0;
    if (!api->export_native(module, "sendto_raw", fn_sendto_raw)) return 0;
    if (!api->export_native(module, "setsockopt", fn_setsockopt)) return 0;
    if (!api->export_native(module, "getsockopt", fn_getsockopt)) return 0;
    if (!api->export_native(module, "set_timeout", fn_set_timeout)) return 0;
    if (!api->export_native(module, "shutdown", fn_shutdown)) return 0;
    if (!api->export_native(module, "recvfrom", fn_recvfrom)) return 0;
    if (!api->export_native(module, "recvfrom_bytes", fn_recvfrom_bytes)) return 0;
    if (!api->export_native(module, "send_all", fn_send_all)) return 0;
    if (!api->export_native(module, "recv_exact", fn_recv_exact)) return 0;
    if (!api->export_native(module, "recv_until", fn_recv_until)) return 0;
    if (!api->export_native(module, "set_nonblocking", fn_set_nonblocking)) return 0;
    if (!api->export_native(module, "select", fn_select)) return 0;
    if (!api->export_native(module, "getsockname", fn_getsockname)) return 0;
    if (!api->export_native(module, "getpeername", fn_getpeername)) return 0;
    if (!api->export_native(module, "set_reuseaddr", fn_set_reuseaddr)) return 0;
    if (!api->export_native(module, "set_keepalive", fn_set_keepalive)) return 0;
    if (!api->export_native(module, "set_nodelay", fn_set_nodelay)) return 0;
    if (!api->export_native(module, "set_broadcast", fn_set_broadcast)) return 0;
    if (!api->export_native(module, "multicast_join", fn_multicast_join)) return 0;
    if (!api->export_native(module, "multicast_leave", fn_multicast_leave)) return 0;
    if (!api->export_native(module, "last_error", fn_last_error)) return 0;

    return 1;
}
