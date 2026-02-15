#include "itsuki.h"
#include <stdarg.h>
#include <setjmp.h>

char* my_strdup(const char* s) { 
    char* d = malloc(strlen(s) + 1); 
    if(d) strcpy(d, s); 
    return d; 
}

unsigned int hash(const char* s) {
    unsigned int h = 5381;
    while(*s) h = ((h << 5) + h) + *s++;
    return h % 512;
}

void hash_insert(HashTable* ht, const char* nombre, int index) {
    unsigned int h = hash(nombre);
    HashNode* node = malloc(sizeof(HashNode));
    strcpy(node->nombre, nombre);
    node->index = index;
    node->next = ht->buckets[h];
    ht->buckets[h] = node;
    
    if (ht == &ht_vars) {
        node->scope_next = current_scope_nodes;
        current_scope_nodes = node;
    } else {
        node->scope_next = NULL;
    }
}

int hash_lookup(HashTable* ht, const char* nombre) {
    unsigned int h = hash(nombre);
    HashNode* node = ht->buckets[h];
    while(node) {
        if(!strcmp(node->nombre, nombre)) return node->index;
        node = node->next;
    }
    return -1;
}

void hash_enter_scope(HashNode** saved_scope) {
    *saved_scope = current_scope_nodes;
    current_scope_nodes = NULL;
}

void hash_exit_scope(HashNode** saved_scope) {
    while(current_scope_nodes) {
        HashNode* n = current_scope_nodes;
        unsigned int h = hash(n->nombre);
        ht_vars.buckets[h] = n->next; 
        current_scope_nodes = n->scope_next;
        free(n);
    }
    current_scope_nodes = *saved_scope;
}

Array* array_crear(int cap) {
    Array* arr = gc_alloc(sizeof(Array), OBJ_ARRAY);
    arr->capacidad = cap > 0 ? cap : 10;
    arr->elementos = malloc(sizeof(Result) * arr->capacidad);
    arr->tamano = 0;
    return arr;
}

void array_agregar(Array* arr, Result val) {
    if(arr->tamano >= arr->capacidad) {
        arr->capacidad *= 2;
        arr->elementos = realloc(arr->elementos, sizeof(Result) * arr->capacidad);
    }
    arr->elementos[arr->tamano++] = val;
}

Result array_obtener(Array* arr, int indice) {
    if(indice >= 0 && indice < arr->tamano) return arr->elementos[indice];
    return (Result){.tipo = TIPO_NULO};
}

void array_establecer(Array* arr, int indice, Result val) {
    if(indice >= 0 && indice < arr->tamano) arr->elementos[indice] = val;
}

Map* map_crear() {
    Map* m = gc_alloc(sizeof(Map), OBJ_MAP);
    memset(m->buckets, 0, sizeof(m->buckets));
    m->tamano = 0;
    return m;
}

void map_establecer(Map* m, const char* clave, Result valor) {
    unsigned int h = hash(clave) % 64;
    MapEntry* e = m->buckets[h];
    while(e) {
        if(!strcmp(e->clave, clave)) { *e->valor = valor; return; }
        e = e->next;
    }
    MapEntry* nu = malloc(sizeof(MapEntry));
    nu->clave = my_strdup(clave);
    nu->valor = malloc(sizeof(Result));
    *nu->valor = valor;
    nu->next = m->buckets[h];
    m->buckets[h] = nu;
    m->tamano++;
}

Result map_obtener(Map* m, const char* clave) {
    unsigned int h = hash(clave) % 64;
    MapEntry* e = m->buckets[h];
    while(e) {
        if(!strcmp(e->clave, clave)) return *e->valor;
        e = e->next;
    }
    return (Result){.tipo = TIPO_NULO};
}

void liberar_resultado(Result r) {
    if (r.obj) return;
}

void liberar_variable(Variable* v) {
    if (v->obj) return;
}

bool repl_mode = false;
jmp_buf error_jmp;
char ultima_linea_repl[1024] = ""; 

bool lsp_mode = false;
jmp_buf lsp_jmp;
char lsp_error_msg[2048];

ProfileData perfil_datos[MAX_FUNCS];
int n_perfil = 0;

const char* obtener_nombre_error(TipoError tipo) {
    switch(tipo) {
        case ERROR_SINTAXIS: return "SyntaxError";
        case ERROR_TIPO: return "TypeError";
        case ERROR_NOMBRE: return "NameError";
        case ERROR_DIVISION_CERO: return "MathError";
        case ERROR_INDICE: return "IndexError";
        case ERROR_ARCHIVO: return "FileError";
        case ERROR_ARGUMENTO: return "ArgumentError";
        case ERROR_SISTEMA: return "SystemError";
        default: return "Error";
    }
}

void lanzar_error(TipoError tipo, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsprintf(buf, fmt, args);
    va_end(args);
    
    if (vm.exception_sp > 0) {
        Result error_obj = {.tipo = TIPO_CADENA, .s = my_strdup(buf)};
        vm_unwind_to_handler(error_obj);
    }
    
    if (try_sp > 0) {
        strcpy(error_pub_msg, buf); 
        longjmp(try_stack[--try_sp].buf, 1);
    }
    
    if(lsp_mode) {
        snprintf(lsp_error_msg, sizeof(lsp_error_msg), "Linea %d: %s", tk.linea, buf);
        longjmp(lsp_jmp, 1);
    }

    if(repl_mode && strlen(ultima_linea_repl) > 0) {
         fprintf(stderr, "  Linea %d: %s\n", tk.linea, ultima_linea_repl);
    }

    fprintf(stderr, "%s: %s\n", obtener_nombre_error(tipo), buf);
    
    if(repl_mode) {
        longjmp(error_jmp, 1);
    } else {
        exit(1);
    }
}

TipoExacto string_a_tipo_exacto(const char* s) {
    if (!strcmp(s, "int8")) return TEX_INT8;
    if (!strcmp(s, "int16")) return TEX_INT16;
    if (!strcmp(s, "int32")) return TEX_INT32;
    if (!strcmp(s, "int64")) return TEX_INT64;
    if (!strcmp(s, "int")) return TEX_INT;
    if (!strcmp(s, "uint8")) return TEX_UINT8;
    if (!strcmp(s, "uint16")) return TEX_UINT16;
    if (!strcmp(s, "uint32")) return TEX_UINT32;
    if (!strcmp(s, "uint64")) return TEX_UINT64;
    if (!strcmp(s, "uint")) return TEX_UINT;
    if (!strcmp(s, "float32")) return TEX_FLOAT32;
    if (!strcmp(s, "float64")) return TEX_FLOAT64;
    if (!strcmp(s, "float")) return TEX_FLOAT;
    if (!strcmp(s, "char")) return TEX_CHAR;
    if (!strcmp(s, "string")) return TEX_STRING;
    if (!strcmp(s, "bool")) return TEX_BOOL;
    return TEX_AUTO;
}

const char* tipo_exacto_a_string(TipoExacto te) {
    switch(te) {
        case TEX_INT8: return "int8";
        case TEX_INT16: return "int16";
        case TEX_INT32: return "int32";
        case TEX_INT64: return "int64";
        case TEX_INT: return "int";
        case TEX_UINT8: return "uint8";
        case TEX_UINT16: return "uint16";
        case TEX_UINT32: return "uint32";
        case TEX_UINT64: return "uint64";
        case TEX_UINT: return "uint";
        case TEX_FLOAT32: return "float32";
        case TEX_FLOAT64: return "float64";
        case TEX_FLOAT: return "float";
        case TEX_CHAR: return "char";
        case TEX_STRING: return "string";
        case TEX_BOOL: return "bool";
        default: return "desconocido";
    }
}

void aplicar_limites_tipo(Result* r, TipoExacto te) {
    if (te == TEX_AUTO) return;
    
    if (te == TEX_STRING) {
        if (r->tipo != TIPO_CADENA) lanzar_error(ERROR_TIPO, "Se esperaba una cadena");
        return;
    }

    if (te == TEX_CHAR) {
        if (r->tipo != TIPO_CADENA || strlen(r->s) != 1) 
            lanzar_error(ERROR_TIPO, "Se esperaba un solo carácter");
        return;
    }

    if (te == TEX_BOOL) {
        r->n = (r->n != 0) ? 1 : 0;
        r->tipo = TIPO_BOOL;
        return;
    }

    if (r->tipo != TIPO_NUMERO && r->tipo != TIPO_BOOL) 
        lanzar_error(ERROR_TIPO, "Se esperaba un valor numérico para este tipo");

    switch (te) {
        case TEX_INT8: r->n = (double)((int8_t)r->n); break;
        case TEX_INT16: r->n = (double)((int16_t)r->n); break;
        case TEX_INT32: r->n = (double)((int32_t)r->n); break;
        case TEX_INT64: r->n = (double)((int64_t)r->n); break;
        case TEX_INT: r->n = (double)((int)r->n); break;
        
        case TEX_UINT8: r->n = (double)((uint8_t)r->n); break;
        case TEX_UINT16: r->n = (double)((uint16_t)r->n); break;
        case TEX_UINT32: r->n = (double)((uint32_t)r->n); break;
        case TEX_UINT64: r->n = (double)((uint64_t)r->n); break;
        case TEX_UINT: r->n = (double)((unsigned int)r->n); break;
        
        case TEX_FLOAT32: r->n = (double)((float)r->n); break;
        case TEX_FLOAT64:
        case TEX_FLOAT:  break;
        default: break;
    }
}

void macro_insert(const char* nombre, const char* reemplazo, TipoExacto te) {
    if (n_m >= MAX_MACROS) return;
    strcpy(macros[n_m].nombre, nombre);
    macros[n_m].reemplazo = my_strdup(reemplazo);
    macros[n_m].tipo_ex = te;
    hash_insert(&ht_macros, nombre, n_m);
    n_m++;
}

Macro* macro_lookup(const char* nombre) {
    int idx = hash_lookup(&ht_macros, nombre);
    if (idx != -1 && macros[idx].nombre[0] != 0) return &macros[idx];
    return NULL;
}
