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
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "itsuki.h"


Result builtin_socket_socket(Result args[], int n_args) {
    int af = (n_args > 0 && args[0].tipo == TIPO_NUMERO) ? (int)args[0].n : AF_INET;
    int type = (n_args > 1 && args[1].tipo == TIPO_NUMERO) ? (int)args[1].n : SOCK_STREAM;
    int proto = 0;
    ObjSocket* sock = (ObjSocket*)gc_alloc(sizeof(ObjSocket), OBJ_SOCKET);
    sock->handle = (SocketHandle)(socket)(af, type, proto);
    sock->is_open = (sock->handle != (SocketHandle)INVALID_SOCKET);
    if (!sock->is_open) lanzar_error(ERROR_EJECUCION, "Fallo socket");
    Result r; r.tipo = TIPO_NULO; r.obj = (Obj*)sock; return r;
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
    }
    if ((bind)((SOCKET)sock->handle, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) lanzar_error(ERROR_EJECUCION, "Fallo bind");
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_socket_listen(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "listen requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int backlog = (n_args > 1) ? (int)args[1].n : 5;
    if ((listen)((SOCKET)sock->handle, backlog) == SOCKET_ERROR) lanzar_error(ERROR_EJECUCION, "Fallo listen");
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_socket_accept(Result args[], int n_args) {
    if (n_args < 1) lanzar_error(ERROR_ARGUMENTO, "accept requiere socket");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    struct sockaddr_in client_addr; int len = sizeof(client_addr);
    SOCKET client_handle = (accept)((SOCKET)sock->handle, (struct sockaddr*)&client_addr, &len);
    if (client_handle == (SOCKET)INVALID_SOCKET) lanzar_error(ERROR_EJECUCION, "Fallo accept");
    ObjSocket* client_obj = (ObjSocket*)gc_alloc(sizeof(ObjSocket), OBJ_SOCKET);
    client_obj->handle = (SocketHandle)client_handle; client_obj->is_open = true;
    Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 2; arr->tamano = 2; arr->elementos = malloc(sizeof(Result) * 2);
    arr->elementos[0] = (Result){.tipo = TIPO_NULO, .obj = (Obj*)client_obj};
    arr->elementos[1] = gc_new_string(inet_ntoa(client_addr.sin_addr));
    return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

Result builtin_socket_connect(Result args[], int n_args) {
    if (n_args < 3) lanzar_error(ERROR_ARGUMENTO, "connect requiere socket, host, port");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* host = args[1].s; int port = (int)args[2].n;
    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
         struct hostent* he = (gethostbyname)(host);
         if (he) memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
    }
    if ((connect)((SOCKET)sock->handle, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) lanzar_error(ERROR_EJECUCION, "Fallo connect");
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_socket_send(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "send requiere socket, data");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    char* data = args[1].s; int sent = (send)((SOCKET)sock->handle, data, strlen(data), 0);
    if (sent == SOCKET_ERROR) lanzar_error(ERROR_EJECUCION, "Fallo send");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
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
    }
    int sent = (sendto)((SOCKET)sock->handle, data, strlen(data), 0, (struct sockaddr*)&sa, sizeof(sa));
    if (sent == SOCKET_ERROR) lanzar_error(ERROR_EJECUCION, "Fallo sendto");
    return (Result){.tipo = TIPO_NUMERO, .n = (double)sent};
}

Result builtin_socket_recv(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recv requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n; char* buf = malloc(buflen + 1);
    int received = (recv)((SOCKET)sock->handle, buf, buflen, 0);
    if (received == SOCKET_ERROR) { free(buf); lanzar_error(ERROR_EJECUCION, "Fallo recv"); }
    buf[received] = 0; Result r = gc_new_string(buf); free(buf); return r;
}

Result builtin_socket_recvfrom(Result args[], int n_args) {
    if (n_args < 2) lanzar_error(ERROR_ARGUMENTO, "recvfrom requiere socket, buflen");
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    int buflen = (int)args[1].n; char* buf = malloc(buflen + 1);
    struct sockaddr_in sa; int sa_len = sizeof(sa);
    int received = (recvfrom)((SOCKET)sock->handle, buf, buflen, 0, (struct sockaddr*)&sa, &sa_len);
    if (received == SOCKET_ERROR) { free(buf); lanzar_error(ERROR_EJECUCION, "Fallo recvfrom"); }
    buf[received] = 0; Array* arr = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = 2; arr->tamano = 2; arr->elementos = malloc(sizeof(Result) * 2);
    arr->elementos[0] = gc_new_string(buf); arr->elementos[1] = gc_new_string(inet_ntoa(sa.sin_addr));
    free(buf); return (Result){.tipo = TIPO_ARRAY, .obj = (Obj*)arr, .a = arr};
}

Result builtin_socket_close(Result args[], int n_args) {
    if (n_args < 1) return (Result){.tipo = TIPO_NULO};
    ObjSocket* sock = (ObjSocket*)args[0].obj;
    if (sock->is_open) {
        #ifdef _WIN32
        closesocket((SOCKET)sock->handle);
        #else
        close((SOCKET)sock->handle);
        #endif
        sock->is_open = false; sock->handle = (SocketHandle)INVALID_SOCKET;
    }
    return (Result){.tipo = TIPO_NULO};
}
