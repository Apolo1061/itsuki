#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/time.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef unsigned int socklen_t;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "itsuki.h"

static bool socket_initialized = false;

static int socket_last_error_code() {
#ifdef _WIN32
    return (int)WSAGetLastError();
#else
    return errno;
#endif
}

static void socket_throw(const char* what) {
    int code = socket_last_error_code();
#ifdef _WIN32
    lanzar_error(ERROR_EJECUCION, "%s (WSA: %d)", what, code);
#else
    lanzar_error(ERROR_EJECUCION, "%s (errno: %d)", what, code);
#endif
}

void socket_init_internal() {
#ifdef _WIN32
    if (!socket_initialized) {
        WSADATA wsa;
        int rc = WSAStartup(MAKEWORD(2,2), &wsa);
        if (rc != 0) {
            lanzar_error(ERROR_EJECUCION, "WSAStartup fallo (WSA: %d)", rc);
        }
        socket_initialized = true;
    }
#endif
}

static uint8_t* array_to_u8_buf(Array* arr, int* out_len) {
    if (!arr || arr->tamano <= 0) {
        *out_len = 0;
        return NULL;
    }
    int len = arr->tamano;
    uint8_t* buf = (uint8_t*)malloc((size_t)len);
    if (!buf) {
        lanzar_error(ERROR_SISTEMA, "Sin memoria");
    }
    for (int i = 0; i < len; i++) {
        if (arr->elementos[i].tipo != TIPO_NUMERO && arr->elementos[i].tipo != TIPO_BOOL) {
            free(buf);
            lanzar_error(ERROR_TIPO, "Se esperaban bytes (numeros 0-255)");
        }
        int v = (int)arr->elementos[i].n;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        buf[i] = (uint8_t)v;
    }
    *out_len = len;
    return buf;
}

static Result new_byte_array_from_buf(const uint8_t* buf, int len) {
    Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = len;
    arr->tamano = len;
    arr->elementos = (len > 0) ? (Result*)malloc(sizeof(Result) * (size_t)len) : NULL;
    for (int i = 0; i < len; i++) {
        arr->elementos[i] = (Result){.tipo = TIPO_NUMERO, .n = (double)buf[i]};
    }
    return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

static Result new_sockaddr_info_array(struct sockaddr_in* sa) {
    Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 2;
    arr->tamano = 2;
    arr->elementos = (Result*)malloc(sizeof(Result) * 2);
    arr->elementos[0] = gc_new_string(inet_ntoa(sa->sin_addr));
    arr->elementos[1] = (Result){.tipo = TIPO_NUMERO, .n = (double)ntohs(sa->sin_port)};
    return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}


Result builtin_socket_socket(Result args[], int n_args) {
    socket_init_internal();
    int af = (n_args > 0 && args[0].tipo == TIPO_NUMERO) ? (int)args[0].n : AF_INET;
    int type = (n_args > 1 && args[1].tipo == TIPO_NUMERO) ? (int)args[1].n : SOCK_STREAM;
    int proto = (n_args > 2 && args[2].tipo == TIPO_NUMERO) ? (int)args[2].n : 0;
#ifdef _WIN32
    if (type == SOCK_RAW) {
        lanzar_error(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
    }
#endif
    ObjSocket* sock = (ObjSocket*)gc_alloc(sizeof(ObjSocket), OBJ_SOCKET);
    sock->handle = (SocketHandle)(socket)(af, type, proto);
    sock->is_open = (sock->handle != (SocketHandle)INVALID_SOCKET);
    if (!sock->is_open) socket_throw("Fallo socket");
    Result r = {0}; r.tipo = TIPO_SOCKET; r.obj = (Obj*)sock; return r;
}

Result builtin_socket_set_hdrincl(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_hdrincl requiere socket, enabled");
#ifdef _WIN32
    lanzar_error(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#endif
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
    int opt = enabled ? 1 : 0;
    if ((setsockopt)((SOCKET)sock->handle, IPPROTO_IP, IP_HDRINCL, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        lanzar_error(ERROR_EJECUCION, "Fallo set_hdrincl");
    }
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_bind(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "bind requiere socket, host, port");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* host = args[1].s;
    int port = (int)args[2].n;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
         struct hostent* he = (gethostbyname)(host);
         if (he) memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
         else lanzar_error(ERROR_NOMBRE, "No se pudo resolver host: %s", host);
    }
    if ((bind)((SOCKET)sock->handle, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) socket_throw("Fallo bind");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_listen(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "listen requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int backlog = (n_args > 1) ? (int)args[1].n : 5;
    if ((listen)((SOCKET)sock->handle, backlog) == SOCKET_ERROR) socket_throw("Fallo listen");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_accept(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "accept requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    struct sockaddr_in client_addr; socklen_t len = sizeof(client_addr);
    SOCKET client_handle = (accept)((SOCKET)sock->handle, (struct sockaddr*)&client_addr, &len);
    if (client_handle == (SOCKET)INVALID_SOCKET) socket_throw("Fallo accept");
    ObjSocket* client_obj = (ObjSocket*)gc_alloc(sizeof(ObjSocket), OBJ_SOCKET);
    client_obj->handle = (SocketHandle)client_handle; client_obj->is_open = true;
    Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 3; arr->tamano = 3; arr->elementos = malloc(sizeof(Result) * 3);
    arr->elementos[0] = (Result){.tipo = TIPO_SOCKET, .obj = (Obj*)client_obj};
    arr->elementos[1] = gc_new_string(inet_ntoa(client_addr.sin_addr));
    arr->elementos[2] = (Result){.tipo = TIPO_NUMERO, .n = (double)ntohs(client_addr.sin_port)};
    return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

Result builtin_socket_connect(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "connect requiere socket, host, port");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* host = args[1].s; int port = (int)args[2].n;
    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    
    if ((connect)((SOCKET)sock->handle, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) socket_throw("Error conexion TCP");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_send(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "send requiere socket, data");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* data = args[1].s; int sent = (send)((SOCKET)sock->handle, data, strlen(data), 0);
    if (sent == SOCKET_ERROR) socket_throw("Fallo send");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_send_bytes(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "send_bytes requiere socket, bytes");
    if (args[1].tipo != TIPO_ARRAY) lanzar_error(ERROR_TIPO, "send_bytes requiere array de bytes");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int len = 0;
    uint8_t* buf = array_to_u8_buf(args[1].a, &len);
    if (len <= 0) return (Result){.tipo = TIPO_NUMERO, .n = 0};
    int sent = (send)((SOCKET)sock->handle, (const char*)buf, len, 0);
    free(buf);
    if (sent == SOCKET_ERROR) socket_throw("Fallo send_bytes");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_send_all_bytes(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "send_all_bytes requiere socket, bytes");
    if (args[1].tipo != TIPO_ARRAY) lanzar_error(ERROR_TIPO, "send_all_bytes requiere array de bytes");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int len = 0;
    uint8_t* buf = array_to_u8_buf(args[1].a, &len);
    int off = 0;
    while (off < len) {
        int sent = (send)((SOCKET)sock->handle, (const char*)(buf + off), len - off, 0);
        if (sent == SOCKET_ERROR) { free(buf); socket_throw("Fallo send_all_bytes"); }
        if (sent <= 0) { free(buf); lanzar_error(ERROR_EJECUCION, "send_all_bytes: conexion cerrada"); }
        off += sent;
    }
    free(buf);
    return (Result){.tipo = TIPO_NUMERO, .n = (double)len};
}

Result builtin_socket_sendto(Result args[], int n_args) {
    if (n_args < 4) lanzar_error(ERROR_ARGUMENTO, "sendto requiere socket, data, host, port");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* data = args[1].s; char* host = args[2].s; int port = (int)args[3].n;
    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
         struct hostent* he = (gethostbyname)(host);
         if (he) memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
         else lanzar_error(ERROR_NOMBRE, "No se pudo resolver host: %s", host);
    }
    int sent = (sendto)((SOCKET)sock->handle, data, strlen(data), 0, (struct sockaddr*)&sa, sizeof(sa));
    if (sent == SOCKET_ERROR) socket_throw("Fallo sendto");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_sendto_bytes(Result args[], int n_args) {
    if (n_args < 4) lanzar_error(ERROR_ARGUMENTO, "sendto_bytes requiere socket, bytes, host, port");
    if (args[1].tipo != TIPO_ARRAY) lanzar_error(ERROR_TIPO, "sendto_bytes requiere array de bytes");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int len = 0;
    uint8_t* buf = array_to_u8_buf(args[1].a, &len);
    char* host = args[2].s; int port = (int)args[3].n;
    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
         struct hostent* he = (gethostbyname)(host);
         if (he) memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
         else { free(buf); lanzar_error(ERROR_NOMBRE, "No se pudo resolver host: %s", host); }
    }
    int sent = (sendto)((SOCKET)sock->handle, (const char*)buf, len, 0, (struct sockaddr*)&sa, sizeof(sa));
    free(buf);
    if (sent == SOCKET_ERROR) socket_throw("Fallo sendto_bytes");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_sendto_raw(Result args[], int n_args) {
    if (n_args < 4) lanzar_error(ERROR_ARGUMENTO, "sendto_raw requiere socket, bytes, host, port");
    if (args[1].tipo != TIPO_ARRAY) lanzar_error(ERROR_TIPO, "sendto_raw requiere array de bytes");
#ifdef _WIN32
    lanzar_error(ERROR_EJECUCION, "Raw sockets no disponibles en Windows");
#endif
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    Array* arr = args[1].a;
    int len = arr->tamano;
    if (len <= 0) return (Result){.tipo = TIPO_NUMERO, .n = 0};
    uint8_t* buf = malloc(len);
    for (int i = 0; i < len; i++) {
        if (arr->elementos[i].tipo != TIPO_NUMERO && arr->elementos[i].tipo != TIPO_BOOL) {
            free(buf);
            lanzar_error(ERROR_TIPO, "sendto_raw requiere numeros 0-255");
        }
        int v = (int)arr->elementos[i].n;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        buf[i] = (uint8_t)v;
    }
    char* host = args[2].s; int port = (int)args[3].n;
    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
         struct hostent* he = (gethostbyname)(host);
         if (he) memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
         else { free(buf); lanzar_error(ERROR_NOMBRE, "No se pudo resolver host: %s", host); }
    }
    int sent = (sendto)((SOCKET)sock->handle, (char*)buf, len, 0, (struct sockaddr*)&sa, sizeof(sa));
    free(buf);
    if (sent == SOCKET_ERROR) socket_throw("Fallo sendto_raw");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_setsockopt(Result args[], int n_args) {
    if (n_args < 4) lanzar_error(ERROR_ARGUMENTO, "setsockopt requiere socket, level, opt, value");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int level = (int)args[1].n;
    int opt = (int)args[2].n;
    int value = (int)args[3].n;
    if ((setsockopt)((SOCKET)sock->handle, level, opt, (char*)&value, sizeof(value)) == SOCKET_ERROR) {
        socket_throw("Fallo setsockopt");
    }
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_getsockopt(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "getsockopt requiere socket, level, opt");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int level = (int)args[1].n;
    int opt = (int)args[2].n;
    int value = 0;
    socklen_t len = sizeof(value);
    if ((getsockopt)((SOCKET)sock->handle, level, opt, (char*)&value, &len) == SOCKET_ERROR) {
        socket_throw("Fallo getsockopt");
    }
    return (Result){.tipo = TIPO_NUMERO, .n = (double)value};
}

Result builtin_socket_set_timeout(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_timeout requiere socket, recv_ms, [send_ms]");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int recv_ms = (int)args[1].n;
    int send_ms = (n_args > 2) ? (int)args[2].n : recv_ms;
#ifdef _WIN32
    DWORD r = (DWORD)recv_ms;
    DWORD s = (DWORD)send_ms;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&r, sizeof(r)) == SOCKET_ERROR) {
        socket_throw("Fallo set_timeout (recv)");
    }
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_SNDTIMEO, (char*)&s, sizeof(s)) == SOCKET_ERROR) {
        socket_throw("Fallo set_timeout (send)");
    }
#else
    struct timeval tr;
    tr.tv_sec = recv_ms / 1000;
    tr.tv_usec = (recv_ms % 1000) * 1000;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_RCVTIMEO, &tr, sizeof(tr)) == SOCKET_ERROR) {
        socket_throw("Fallo set_timeout (recv)");
    }
    struct timeval ts;
    ts.tv_sec = send_ms / 1000;
    ts.tv_usec = (send_ms % 1000) * 1000;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_SNDTIMEO, &ts, sizeof(ts)) == SOCKET_ERROR) {
        socket_throw("Fallo set_timeout (send)");
    }
#endif
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_shutdown(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "shutdown requiere socket, how");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int how = (int)args[1].n;
    if ((shutdown)((SOCKET)sock->handle, how) == SOCKET_ERROR) {
        socket_throw("Fallo shutdown");
    }
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_recv(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recv requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n; char* buf = malloc(buflen + 1);
    int received = (recv)((SOCKET)sock->handle, buf, buflen, 0);
    if (received == SOCKET_ERROR) { free(buf); socket_throw("Fallo recv"); }
    buf[received] = 0; Result r = gc_new_string(buf); free(buf); return r;
}

Result builtin_socket_recv_bytes(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recv_bytes requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    uint8_t* buf = (buflen > 0) ? (uint8_t*)malloc((size_t)buflen) : NULL;
    int received = (buflen > 0) ? (recv)((SOCKET)sock->handle, (char*)buf, buflen, 0) : 0;
    if (received == SOCKET_ERROR) { free(buf); socket_throw("Fallo recv_bytes"); }
    Result r = new_byte_array_from_buf(buf, received);
    free(buf);
    return r;
}

Result builtin_socket_recv_exact_bytes(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recv_exact_bytes requiere socket, n");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int want = (int)args[1].n;
    if (want < 0) want = 0;
    uint8_t* buf = (want > 0) ? (uint8_t*)malloc((size_t)want) : NULL;
    int off = 0;
    while (off < want) {
        int got = (recv)((SOCKET)sock->handle, (char*)(buf + off), want - off, 0);
        if (got == SOCKET_ERROR) { free(buf); socket_throw("Fallo recv_exact_bytes"); }
        if (got == 0) { free(buf); lanzar_error(ERROR_EJECUCION, "recv_exact_bytes: EOF"); }
        off += got;
    }
    Result r = new_byte_array_from_buf(buf, want);
    free(buf);
    return r;
}

static int find_subseq_u8(const uint8_t* hay, int hay_len, const uint8_t* needle, int needle_len) {
    if (!hay || !needle || needle_len <= 0) return -1;
    if (needle_len > hay_len) return -1;
    for (int i = 0; i <= hay_len - needle_len; i++) {
        int ok = 1;
        for (int j = 0; j < needle_len; j++) {
            if (hay[i + j] != needle[j]) { ok = 0; break; }
        }
        if (ok) return i;
    }
    return -1;
}

Result builtin_socket_recv_until_bytes(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "recv_until_bytes requiere socket, delim_bytes, max_bytes");
    if (args[1].tipo != TIPO_ARRAY) lanzar_error(ERROR_TIPO, "recv_until_bytes requiere array delim");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int delim_len = 0;
    uint8_t* delim = array_to_u8_buf(args[1].a, &delim_len);
    if (delim_len <= 0) { free(delim); lanzar_error(ERROR_ARGUMENTO, "recv_until_bytes: delim vacio"); }

    int max_bytes = (int)args[2].n;
    if (max_bytes <= 0) max_bytes = 1024 * 1024;

    int chunk_size = 4096;
    if (n_args > 3 && (args[3].tipo == TIPO_NUMERO || args[3].tipo == TIPO_BOOL)) {
        chunk_size = (int)args[3].n;
        if (chunk_size <= 0) chunk_size = 4096;
        if (chunk_size > 65536) chunk_size = 65536;
    }

    uint8_t* buf = NULL;
    int buf_len = 0;

    while (buf_len < max_bytes) {
        uint8_t tmp[65536];
        int want = chunk_size;
        if (want > (max_bytes - buf_len)) want = (max_bytes - buf_len);
        int got = (recv)((SOCKET)sock->handle, (char*)tmp, want, 0);
        if (got == SOCKET_ERROR) { free(buf); free(delim); socket_throw("Fallo recv_until_bytes"); }
        if (got == 0) { free(buf); free(delim); lanzar_error(ERROR_EJECUCION, "recv_until_bytes: EOF"); }

        uint8_t* new_buf = (uint8_t*)realloc(buf, (size_t)(buf_len + got));
        if (!new_buf) { free(buf); free(delim); lanzar_error(ERROR_SISTEMA, "Sin memoria"); }
        buf = new_buf;
        memcpy(buf + buf_len, tmp, (size_t)got);
        buf_len += got;

        int ix = find_subseq_u8(buf, buf_len, delim, delim_len);
        if (ix != -1) {
            int out_len = ix + delim_len;
            Result r = new_byte_array_from_buf(buf, out_len);
            free(buf);
            free(delim);
            return r;
        }
    }

    free(buf);
    free(delim);
    lanzar_error(ERROR_EJECUCION, "recv_until_bytes: excedio max_bytes");
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_socket_recvfrom(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recvfrom requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n; char* buf = malloc(buflen + 1);
    struct sockaddr_in sa; socklen_t sa_len = sizeof(sa);
    int received = (recvfrom)((SOCKET)sock->handle, buf, buflen, 0, (struct sockaddr*)&sa, &sa_len);
    if (received == SOCKET_ERROR) { free(buf); socket_throw("Fallo recvfrom"); }
    buf[received] = 0; Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 3; arr->tamano = 3; arr->elementos = malloc(sizeof(Result) * 3);
    arr->elementos[0] = gc_new_string(buf);
    arr->elementos[1] = gc_new_string(inet_ntoa(sa.sin_addr));
    arr->elementos[2] = (Result){.tipo = TIPO_NUMERO, .n = (double)ntohs(sa.sin_port)};
    free(buf); return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

Result builtin_socket_recvfrom_bytes(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recvfrom_bytes requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n;
    if (buflen < 0) buflen = 0;
    uint8_t* buf = (buflen > 0) ? (uint8_t*)malloc((size_t)buflen) : NULL;
    struct sockaddr_in sa; socklen_t sa_len = sizeof(sa);
    int received = (buflen > 0) ? (recvfrom)((SOCKET)sock->handle, (char*)buf, buflen, 0, (struct sockaddr*)&sa, &sa_len) : 0;
    if (received == SOCKET_ERROR) { free(buf); socket_throw("Fallo recvfrom_bytes"); }

    Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 3;
    arr->tamano = 3;
    arr->elementos = (Result*)malloc(sizeof(Result) * 3);
    arr->elementos[0] = new_byte_array_from_buf(buf, received);
    arr->elementos[1] = gc_new_string(inet_ntoa(sa.sin_addr));
    arr->elementos[2] = (Result){.tipo = TIPO_NUMERO, .n = (double)ntohs(sa.sin_port)};
    free(buf);
    return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

Result builtin_socket_close(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "close requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    if (sock->is_open) {
        #ifdef _WIN32
        closesocket(sock->handle);
        #else
        close(sock->handle);
        #endif
        sock->is_open = false;
    }
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_set_nonblocking(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_nonblocking requiere socket, enabled");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
#ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    if (ioctlsocket((SOCKET)sock->handle, FIONBIO, &mode) != 0) socket_throw("Fallo set_nonblocking");
#else
    int fd = (int)sock->handle;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) socket_throw("Fallo set_nonblocking (get)");
    if (enabled) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) socket_throw("Fallo set_nonblocking (set)");
#endif
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_select(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "select requiere read_socks, write_socks, timeout_ms");
    Array* read_in = (args[0].tipo == TIPO_ARRAY) ? args[0].a : NULL;
    Array* write_in = (args[1].tipo == TIPO_ARRAY) ? args[1].a : NULL;
    int timeout_ms = (args[2].tipo == TIPO_NUMERO || args[2].tipo == TIPO_BOOL) ? (int)args[2].n : -1;

    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

#ifndef _WIN32
    int maxfd = 0;
#endif
    if (read_in) {
        for (int i = 0; i < read_in->tamano; i++) {
            if (read_in->elementos[i].tipo != TIPO_SOCKET) continue;
            ObjSocket* s = (ObjSocket*)read_in->elementos[i].obj;
            SOCKET h = (SOCKET)s->handle;
            FD_SET(h, &rfds);
#ifndef _WIN32
            if ((int)h > maxfd) maxfd = (int)h;
#endif
        }
    }
    if (write_in) {
        for (int i = 0; i < write_in->tamano; i++) {
            if (write_in->elementos[i].tipo != TIPO_SOCKET) continue;
            ObjSocket* s = (ObjSocket*)write_in->elementos[i].obj;
            SOCKET h = (SOCKET)s->handle;
            FD_SET(h, &wfds);
#ifndef _WIN32
            if ((int)h > maxfd) maxfd = (int)h;
#endif
        }
    }

    struct timeval tv;
    struct timeval* ptv = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

#ifdef _WIN32
    int rc = (select)(0, &rfds, &wfds, NULL, ptv);
#else
    int rc = (select)(maxfd + 1, &rfds, &wfds, NULL, ptv);
#endif
    if (rc == SOCKET_ERROR) socket_throw("Fallo select");

    Array* read_ready = array_crear(read_in ? read_in->tamano : 0);
    Array* write_ready = array_crear(write_in ? write_in->tamano : 0);

    if (read_in) {
        for (int i = 0; i < read_in->tamano; i++) {
            if (read_in->elementos[i].tipo != TIPO_SOCKET) continue;
            ObjSocket* s = (ObjSocket*)read_in->elementos[i].obj;
            if (FD_ISSET((SOCKET)s->handle, &rfds)) {
                array_agregar(read_ready, (Result){.tipo = TIPO_SOCKET, .obj = (Obj*)s});
            }
        }
    }
    if (write_in) {
        for (int i = 0; i < write_in->tamano; i++) {
            if (write_in->elementos[i].tipo != TIPO_SOCKET) continue;
            ObjSocket* s = (ObjSocket*)write_in->elementos[i].obj;
            if (FD_ISSET((SOCKET)s->handle, &wfds)) {
                array_agregar(write_ready, (Result){.tipo = TIPO_SOCKET, .obj = (Obj*)s});
            }
        }
    }

    Array* out = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    out->capacidad = 2;
    out->tamano = 2;
    out->elementos = (Result*)malloc(sizeof(Result) * 2);
    out->elementos[0] = (Result){.tipo = TIPO_ARRAY, .a = read_ready, .obj = (Obj*)read_ready};
    out->elementos[1] = (Result){.tipo = TIPO_ARRAY, .a = write_ready, .obj = (Obj*)write_ready};
    return (Result){.tipo = TIPO_ARRAY, .a = out, .obj = (Obj*)out};
}

Result builtin_socket_getsockname(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "getsockname requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    if ((getsockname)((SOCKET)sock->handle, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) socket_throw("Fallo getsockname");
    return new_sockaddr_info_array(&sa);
}

Result builtin_socket_getpeername(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "getpeername requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    if ((getpeername)((SOCKET)sock->handle, (struct sockaddr*)&sa, &len) == SOCKET_ERROR) socket_throw("Fallo getpeername");
    return new_sockaddr_info_array(&sa);
}

Result builtin_socket_set_reuseaddr(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_reuseaddr requiere socket, enabled");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
    int opt = enabled ? 1 : 0;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) socket_throw("Fallo set_reuseaddr");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_set_keepalive(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_keepalive requiere socket, enabled");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
    int opt = enabled ? 1 : 0;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) socket_throw("Fallo set_keepalive");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_set_nodelay(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_nodelay requiere socket, enabled");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
    int opt = enabled ? 1 : 0;
    if ((setsockopt)((SOCKET)sock->handle, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) socket_throw("Fallo set_nodelay");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_set_broadcast(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "set_broadcast requiere socket, enabled");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int enabled = (args[1].tipo == TIPO_NUMERO || args[1].tipo == TIPO_BOOL) ? (args[1].n != 0) : 0;
    int opt = enabled ? 1 : 0;
    if ((setsockopt)((SOCKET)sock->handle, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) socket_throw("Fallo set_broadcast");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_multicast_join(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "multicast_join requiere socket, group, [iface]");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* group = args[1].s;
    char* iface = (n_args > 2 && args[2].tipo == TIPO_CADENA) ? args[2].s : "0.0.0.0";

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = inet_addr(iface);
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE) lanzar_error(ERROR_ARGUMENTO, "Grupo multicast invalido");

    if ((setsockopt)((SOCKET)sock->handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) socket_throw("Fallo multicast_join");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_multicast_leave(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "multicast_leave requiere socket, group, [iface]");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* group = args[1].s;
    char* iface = (n_args > 2 && args[2].tipo == TIPO_CADENA) ? args[2].s : "0.0.0.0";

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = inet_addr(iface);
    if (mreq.imr_multiaddr.s_addr == INADDR_NONE) lanzar_error(ERROR_ARGUMENTO, "Grupo multicast invalido");

    if ((setsockopt)((SOCKET)sock->handle, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) socket_throw("Fallo multicast_leave");
    return (Result){.tipo = TIPO_NUMERO, .n = 0};
}

Result builtin_socket_last_error(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return (Result){.tipo = TIPO_NUMERO, .n = (double)socket_last_error_code()};
}
