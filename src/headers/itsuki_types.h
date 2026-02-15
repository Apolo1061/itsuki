#ifndef ITSUKI_TYPES_H
#define ITSUKI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    TIPO_NUMERO, TIPO_CADENA, TIPO_ARRAY, TIPO_MAP, TIPO_BOOL,
    TIPO_INSTANCIA, TIPO_CLASE, TIPO_NULO, TIPO_FUNCION,
    TIPO_CLAUSURA, TIPO_ENUM, TIPO_MODULO, TIPO_SOCKET
} TipoDato;

typedef enum {
    OBJ_STRING, OBJ_ARRAY, OBJ_MAP, OBJ_INSTANCIA,
    OBJ_CLAUSURA, OBJ_UPVALUE, OBJ_ENUM_DEF, OBJ_ENUM_VAL,
    OBJ_MODULO, OBJ_SOCKET
} ObjType;

typedef struct Obj {
    ObjType type;
    bool is_marked;
    bool is_freed;
    struct Obj* next;
} Obj;

typedef struct {
    struct Obj obj;
    char* s;
} ObjString;

typedef struct Result {
    union {
        double n;
        char* s;
        struct Array* a;
        struct Map* m;
        struct Instancia* inst;
        int func_index;
        int clase_index;
        struct Closure* clausura;
        struct EnumValor* enum_val;
    };
    struct EnumDef* enum_def;
    TipoDato tipo;
    int tipo_ex; 
    struct Obj* obj;
} Result;

typedef struct Array {
    struct Obj obj;
    struct Result* elementos;
    int capacidad;
    int tamano;
} Array;

typedef struct MapEntry {
    char* clave;
    struct Result* valor;
    struct MapEntry* next;
} MapEntry;

typedef struct Map {
    struct Obj obj;
    MapEntry* buckets[64];
    int tamano;
} Map;

typedef struct {
    struct Obj obj;
    char path[256];
    struct Map* exports;
} ObjModulo;

#ifdef _WIN32
typedef uintptr_t SocketHandle;
#else
typedef int SocketHandle;
#endif

typedef struct {
    struct Obj obj;
    SocketHandle handle;
    bool is_open;
} ObjSocket;

typedef enum {
    TEX_AUTO,
    TEX_INT8, TEX_INT16, TEX_INT32, TEX_INT64, TEX_INT,
    TEX_UINT8, TEX_UINT16, TEX_UINT32, TEX_UINT64, TEX_UINT,
    TEX_FLOAT32, TEX_FLOAT64, TEX_FLOAT,
    TEX_CHAR, TEX_STRING, TEX_BOOL
} TipoExacto;

typedef enum {
    ERROR_SINTAXIS, ERROR_TIPO, ERROR_NOMBRE, ERROR_DIVISION_CERO,
    ERROR_INDICE, ERROR_ARCHIVO, ERROR_ARGUMENTO, ERROR_SISTEMA, ERROR_MAXIT,
    ERROR_EJECUCION
} TipoError;


void* gc_alloc(size_t size, ObjType type);
Result gc_new_string(const char* s);
void lanzar_error(TipoError tipo, const char* fmt, ...);

#endif
