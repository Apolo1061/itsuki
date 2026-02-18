#ifndef ITSUKI_H
#define ITSUKI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>
#include <stdint.h>

#define ITSUKI_VERSION "4.5.3"

#define MAX_VARS 2048
#define MAX_FUNCS 512
#define MAX_MACROS 256
#define MAX_ID_LEN 64
#define MAX_TRY_DEPTH 128
#define MAX_FUNC_PARAMS 64

typedef struct {
    jmp_buf buf;
    int catch_token_index;
} TryFrame;

extern TryFrame try_stack[MAX_TRY_DEPTH];
extern int try_sp;
extern char error_pub_msg[1024];

typedef enum {
    TOKEN_SEA, TOKEN_LET, TOKEN_VAR, TOKEN_CONST, TOKEN_PRINT, TOKEN_LEER,
    TOKEN_FUNCION, TOKEN_RETORNAR, TOKEN_CLASE, TOKEN_NUEVA, TOKEN_HEREDA, TOKEN_IMPORTAR, TOKEN_EXPORTAR, TOKEN_DESDE, TOKEN_COMO,
    TOKEN_IDENTIFICADOR, TOKEN_NUMERO, TOKEN_CADENA,
    TOKEN_IGUAL, TOKEN_MAS, TOKEN_MENOS, TOKEN_MULT, TOKEN_DIV,
    TOKEN_PUNTO, TOKEN_COMA, TOKEN_PAR_IZQ, TOKEN_PAR_DER, TOKEN_LLAVE_IZQ, TOKEN_LLAVE_DER,
    TOKEN_MENOR, TOKEN_MAYOR, TOKEN_MENOR_IGUAL, TOKEN_MAYOR_IGUAL,
    TOKEN_IGUAL_IGUAL, TOKEN_DIFERENTE,
    TOKEN_Y, TOKEN_O, TOKEN_NO,
    TOKEN_SI,
    TOKEN_SINO,
    TOKEN_MIENTRAS,
    TOKEN_PARA,
    TOKEN_EN,
    TOKEN_ROMPER,
    TOKEN_CONTINUAR,
    TOKEN_INTENTAR,
    TOKEN_CAPTURAR,
    TOKEN_FINALMENTE,
    TOKEN_LANZAR,
    TOKEN_CORCHETE_IZQ,
    TOKEN_CORCHETE_DER,
    TOKEN_DOS_PUNTOS,
    TOKEN_MODULO,
    TOKEN_NULO,
    TOKEN_FSTRING,
    TOKEN_ESTATICO, TOKEN_PRIVADO, TOKEN_PUBLICO, TOKEN_SUPER,
    TOKEN_ARROBA, TOKEN_LAMBDA,
    TOKEN_ENUM, TOKEN_CASO, TOKEN_ES,
    TOKEN_C_INCLUIR, TOKEN_C_EXTERN,
    TOKEN_VERDADERO, TOKEN_FALSO,
    TOKEN_PUNTO_COMA,
    TOKEN_EOF, TOKEN_ERROR
} TipoToken;

typedef struct {
    TipoToken tipo;
    char* valor;
    int linea;
} Token;

#include "itsuki_types.h"

typedef struct Instancia Instancia;

typedef struct Variable {
    char nombre[MAX_ID_LEN];
    double val_num;
    char* val_str;
    Array* val_array;
    Map* val_map;
    Instancia* val_inst;
    struct EnumValor* enum_val;
    struct EnumDef* enum_def;
    int clase_index;
    Obj* obj;
    bool es_publico;
    uint8_t mut;
    TipoDato tipo;
    TipoExacto tipo_ex;
} Variable;

typedef enum {
    MUT_VAR = 0,
    MUT_LET = 1,
    MUT_CONST = 2
} VarMut;

typedef struct Clase {
    char nombre[MAX_ID_LEN];
    bool es_publico;
    char** propiedades;
    bool* props_privadas;
    int* props_owner;
    int n_propiedades;
    int* indices_metodos;
    bool* metodos_privados;
    bool* metodos_estaticos;
    int n_metodos;
    struct Clase* clase_padre;

    char** nombres_estaticos;
    Result* valores_estaticos;
    bool* estaticos_privados;
    int n_estaticos;
} Clase;

typedef enum {
    AST_NUMERO,
    AST_CADENA,
    AST_IDENTIFICADOR,
    AST_BINOP,
    AST_UNOP,
    AST_ASIGNACION,
    AST_ASIGNACION_INDICE,
    AST_LLAMADA,
    AST_SI,
    AST_MIENTRAS,
    AST_PARA,
    AST_FUNCION,
    AST_RETORNAR,
    AST_BLOQUE,
    AST_ARRAY,
    AST_INDICE,
    AST_MAPA,
    AST_CLASE,
    AST_NUEVA,
    AST_ACCESO,
    AST_METODO,
    AST_LAMBDA,
    AST_COMPREHENSION,
    AST_ENUM,
    AST_TRY_CATCH,
    AST_LANZAR,
    AST_IMPORTAR,
    AST_C_INCLUIR,
    AST_C_EXTERN,
    AST_ROMPER,
    AST_CONTINUAR,
    AST_NULO
} TipoNodoAST;

typedef struct NodoAST {
    TipoNodoAST tipo;
    int linea;
    TipoExacto tipo_ex;
    bool es_estatico;
    bool es_privado;
    bool es_publico;

    union {
        double numero;
        char* cadena;
        char* identificador;

        struct {
            struct NodoAST* izquierda;
            struct NodoAST* derecha;
            int operador;
        } binop;

        struct {
            struct NodoAST* operando;
            int operador;
        } unop;

        struct {
            char* nombre;
            struct NodoAST* receptor;
            struct NodoAST* valor;
            bool es_declaracion;
            uint8_t var_mut;
        } asignacion;

        struct {
            struct NodoAST* receptor;
            struct NodoAST* indice;
            struct NodoAST* valor;
        } asignacion_indice;

        struct {
            char* nombre;
            struct NodoAST* receptor;
            struct NodoAST** args;
            int n_args;
        } llamada;

        struct {
            struct NodoAST* condicion;
            struct NodoAST* bloque_si;
            struct NodoAST* bloque_sino;
            struct NodoAST** elif_conds;
            struct NodoAST** elif_bloques;
            int n_elifs;
        } si;

        struct {
            struct NodoAST* condicion;
            struct NodoAST* cuerpo;
        } mientras;

        struct {
            char* var_nombre;
            struct NodoAST* inicio;
            struct NodoAST* fin;
            struct NodoAST* paso;
            struct NodoAST* cuerpo;
        } para;

        struct {
            char* nombre;
            char** params;
            TipoExacto* param_tipos;
            int n_params;
            struct NodoAST* cuerpo;
            TipoExacto tipo_retorno;
            struct NodoAST** decoradores;
            int n_decoradores;
        } funcion;

        struct {
            struct NodoAST** sentencias;
            int count;
        } bloque;

        struct {
            struct NodoAST** elementos;
            int count;
        } array;

        struct {
            struct NodoAST** llaves;
            struct NodoAST** valores;
            int count;
        } mapa;

        struct {
            char* nombre;
            char* padre;
            struct NodoAST** miembros;
            int n_miembros;
        } clase;

        struct {
            struct NodoAST* expresion;
            char** variables;
            int n_variables;
            struct NodoAST* iterable;
            struct NodoAST* condicion;
            bool es_dict;
        } comprehension;

        struct {
            char nombre[MAX_ID_LEN];
            char** variantes;
            int* n_campos_variante;
            int n_variantes;
        } enumm;

        struct {
            char* header;
        } c_incluir;

        struct {
            char* nombre;
            char* retorno;
            char** param_nombres;
            char** param_tipos;
            int n_params;
        } c_extern;
        struct {
            struct NodoAST* bloque_try;
            char var_error[MAX_ID_LEN];
            struct NodoAST* bloque_catch;
            struct NodoAST* bloque_finally;
        } try_catch;
        struct {
            struct NodoAST* expresion;
        } lanzar;
        struct {
            char* path;
            char* alias;
            char** nombres;
            int n_nombres;
        } importar;
    } datos;
} NodoAST;

struct Instancia {
    Clase* clase;
    Result* valores_propiedades;
};

NodoAST* crear_nodo_numero(double n);
NodoAST* crear_nodo_cadena(const char* s);
NodoAST* crear_nodo_identificador(const char* id);
NodoAST* crear_nodo_binop(NodoAST* izq, int op, NodoAST* der);
NodoAST* crear_nodo_unop(int op, NodoAST* operando);
NodoAST* crear_nodo_asignacion(const char* nombre, NodoAST* valor);
NodoAST* crear_nodo_llamada(const char* nombre, NodoAST** args, int n_args);
NodoAST* crear_nodo_si(NodoAST* condicion, NodoAST* bloque_si, NodoAST* bloque_sino, NodoAST** elif_conds, NodoAST** elif_bloques, int n_elifs);
NodoAST* crear_nodo_mientras(NodoAST* condicion, NodoAST* cuerpo);
NodoAST* crear_nodo_para(const char* var, NodoAST* inicio, NodoAST* fin, NodoAST* paso, NodoAST* cuerpo);
NodoAST* crear_nodo_funcion(const char* nombre, char** params, TipoExacto* param_tipos, int n_params, NodoAST* cuerpo, TipoExacto tipo_retorno);
NodoAST* crear_nodo_bloque(NodoAST** sentencias, int count);
NodoAST* crear_nodo_array(NodoAST** elementos, int count);
NodoAST* crear_nodo_mapa(NodoAST** llaves, NodoAST** valores, int count);
NodoAST* crear_nodo_nulo();

typedef struct Upvalue {
    struct Obj obj;
    Result* location;
    Result closed;
    struct Upvalue* next;
} Upvalue;

typedef struct Closure {
    struct Obj obj;
    int func_index;
    Upvalue** upvalues;
    char** nombres_upvalues;
    int n_upvalues;
} Closure;

typedef enum {
    OP_CONSTANTE,
    OP_NULO,
    OP_VERDADERO,
    OP_FALSO,
    OP_POP,
    OP_DEFINIR_GLOBAL,
    OP_OBTENER_GLOBAL,
    OP_ESTABLECER_GLOBAL,
    OP_OBTENER_LOCAL,
    OP_ESTABLECER_LOCAL,
    OP_SUMA, OP_RESTA, OP_MULT, OP_DIV,
    OP_NO, OP_IGUAL, OP_MAYOR, OP_MENOR,
    OP_SALTAR,
    OP_SALTAR_SI_FALSO,
    OP_LLAMAR,
    OP_COLA_LLAMAR,
    OP_MODULO,
    OP_PRINT,
    OP_RETORNAR,
    OP_CLASE,
    OP_NUEVA,
    OP_OBTENER_PROPIEDAD,
    OP_ESTABLECER_PROPIEDAD,
    OP_INVOQUE_METODO,
    OP_SUPER,
    OP_CLOSURE,
    OP_OBTENER_UPVALUE,
    OP_ESTABLECER_UPVALUE,
    OP_ITER_INIT,
    OP_ITER_NEXT,
    OP_ITER_DONE,
    OP_ARRAY_APPEND,
    OP_ARRAY_CREAR,
    OP_MAP_CREAR,
    OP_ESTABLECER_INDICE,
    OP_IMPORTAR,
    OP_LANZAR_ERROR,
    OP_DEFINIR_CON_TIPO,
    OP_ENUM,
    OP_ENUM_INSTANCIA,
    OP_ES_TIPO,
    OP_TRY_BEGIN,
    OP_TRY_END,
    OP_LANZAR,
    OP_FINALLY_END,
    OP_MARCAR_EXPORT,
    OP_MARCAR_MUT,
    OP_NOP,
    OP_CONST_SUMA,
    OP_CONST_RESTA,
    OP_CONST_MULT,
    OP_CONST_DIV
} OpCode;

typedef struct {
    uint8_t* codigo;
    int capacidad;
    int contador;
    struct Result* constantes;
    int capacidad_constantes;
    int contador_constantes;
    int* cache_global_idx;
    uint32_t* cache_global_ver;
    int cache_size;
    struct Clase** cache_prop_cls;
    const char** cache_prop_name;
    int* cache_prop_index;
    uint8_t* cache_prop_kind;
    struct Clase** cache_invoke_cls;
    int* cache_invoke_func;
    uint8_t* cache_invoke_kind;
} Chunk;

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    struct Result pila[256];
    struct Result* pila_tope;
    struct Closure* clausura_actual;
    bool debug_mode;
    bool step_mode;
    int* breakpoints;
    int n_breakpoints;
    bool profiling_mode;
    bool export_proximo;
    uint8_t mut_proximo;
    struct Array* cli_args;
    struct {
        Result iterable;
        int index;
        int size;
    } iterator_state;
    struct {
        uint8_t* catch_ip;
        uint8_t* finally_ip;
        int pila_sp;
    } exception_stack[MAX_TRY_DEPTH];
    int exception_sp;
    jmp_buf recover_jmp;
    bool hay_error_pendiente;
    Result error_pendiente;
    struct Map* modulos_cargados;
    bool manual_memory_mode;
    uint32_t ht_vars_version;
    bool jit_enabled;
    int jit_hot_threshold;
} VM;

extern VM vm;

typedef struct {
    char nombre_func[MAX_ID_LEN];
    double tiempo_total;
    long llamadas;
} ProfileData;

extern ProfileData perfil_datos[MAX_FUNCS];
extern int n_perfil;

typedef struct {
    char nombre[MAX_ID_LEN];
    int pos;
    int token_index;
    char* source;
    char params[MAX_FUNC_PARAMS][MAX_ID_LEN];
    TipoExacto param_tipos[MAX_FUNC_PARAMS];
    int n_params;
    bool es_publico;
    struct NodoAST* cuerpo_ast;
    Chunk* chunk_bytecode;
    TipoExacto tipo_retorno;
    void* jit_ptr;
    int jit_size;
    int jit_calls;
    int jit_state;
} Funcion;

typedef struct {
    char nombre[MAX_ID_LEN];
    int n_campos;
} EnumVariante;

typedef struct EnumDef {
    struct Obj obj;
    char nombre[MAX_ID_LEN];
    EnumVariante variantes[16];
    int n_variantes;
} EnumDef;

typedef struct EnumValor {
    struct Obj obj;
    EnumDef* definicion;
    int variante_index;
    Result* valores;
} EnumValor;

void free_ast(struct NodoAST* nodo);

NodoAST* val();
NodoAST* unary();
NodoAST* term();
NodoAST* expr();
NodoAST* comparacion();
NodoAST* logica();
NodoAST* parse_stmt();
NodoAST* parse_block();

Result evaluar_ast(NodoAST* n);
Result llamar_funcion_usuario(int f_idx, Result args[], int n_args);

typedef struct HashNode {
    char nombre[MAX_ID_LEN];
    int index;
    struct HashNode* next;
    struct HashNode* scope_next;
} HashNode;

typedef struct {
    HashNode* buckets[512];
} HashTable;

typedef struct {
    char nombre[MAX_ID_LEN];
    char* reemplazo;
    TipoExacto tipo_ex;
} Macro;

extern Macro macros[MAX_MACROS];
extern int n_m;
extern HashTable ht_macros;

typedef struct {
    Token* tokens;
    int capacity;
    int count;
    int current;
} TokenStream;

#define POOL_SIZE 65536
typedef struct {
    char buffer[POOL_SIZE];
    int offset;
} StringPool;

typedef struct {
    bool condition_met;
    bool is_active;
} PPLevel;

typedef struct {
    const char* f;
    int p;
    PPLevel pp_stack[32];
    int pp_depth;
} Lexer;

extern Variable vars[MAX_VARS];
extern int n_v;
extern Funcion funcs[MAX_FUNCS];
extern int n_f;
extern int ln;
extern bool repl_mode;
extern jmp_buf error_jmp;
extern char ultima_linea_repl[1024];
extern Lexer lx;
extern Token tk;
extern Clase clases[MAX_FUNCS];
extern int n_clases;
extern HashTable ht_vars;
extern HashTable ht_funcs;
extern HashTable ht_clases;
extern TokenStream* ts;
extern Result last_res;
extern bool returning;
extern bool debe_romper;
extern bool debe_continuar;
extern HashNode* current_scope_nodes;

char* my_strdup(const char* s);
unsigned int hash(const char* s);
void hash_insert(HashTable* ht, const char* nombre, int index);
int hash_lookup(HashTable* ht, const char* nombre);
void hash_enter_scope(HashNode** saved_scope);
void hash_exit_scope(HashNode** saved_scope);

Token next_tk(Lexer* l);
TokenStream* tokenize_all(const char* source);
void free_token_stream(TokenStream* stream);
void adv();
void leap();

void exec();
void main_exec(const char* src);
extern struct Obj* lista_objetos;
extern int contador_objetos;
extern int umbral_gc;

void* gc_alloc(size_t size, ObjType type);
void gc_marcar();
void liberar_objeto(struct Obj* obj);
void gc_limpiar();
void gc_recolectar();
Result gc_new_string(const char* s);
Result gc_new_array();

void vm_init();
void vm_free();
Result vm_ejecutar(Chunk* chunk);
bool jit_compilar_funcion(int f_idx);
bool jit_ejecutar_funcion(int f_idx, Result args[], int n_args, Result* out);

void guardar_bytecode(Chunk* chunk, const char* filename);
Chunk* cargar_bytecode(const char* filename);

Chunk* compilar_a_bytecode(NodoAST* nodo);
void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void escribir_chunk(Chunk* chunk, uint8_t byte);
int agregar_constante(Chunk* chunk, Result valor);

void itsuki_init();
void itsuki_execute(const char* src);

Array* array_crear(int cap);
void array_agregar(Array* arr, Result val);
Result array_obtener(Array* arr, int indice);
void array_establecer(Array* arr, int indice, Result val);

Map* map_crear();
void map_establecer(Map* m, const char* clave, Result valor);
Result map_obtener(Map* m, const char* clave);

void liberar_resultado(Result r);
void liberar_variable(Variable* v);

bool es_funcion_builtin(const char* nombre);
Result ejecutar_builtin(const char* nombre, Result args[], int n_args);
void registrar_builtins();
Result result_to_string_gc(Result r);
ObjModulo* itsuki_ext_cargar_modulo_nativo(const char* path_orig);
bool es_funcion_nativa_proxy(const char* nombre);
Result ejecutar_nativa_proxy(const char* nombre, Result args[], int n_args);

const char* obtener_nombre_error(TipoError tipo);

Result vm_ejecutar(Chunk* chunk);
void vm_unwind_to_handler(Result error);
void lanzar_error(TipoError tipo, const char* fmt, ...);

Result builtin_socket_socket(Result args[], int n_args);
Result builtin_socket_bind(Result args[], int n_args);
Result builtin_socket_listen(Result args[], int n_args);
Result builtin_socket_accept(Result args[], int n_args);
Result builtin_socket_connect(Result args[], int n_args);
Result builtin_socket_send(Result args[], int n_args);
Result builtin_socket_send_bytes(Result args[], int n_args);
Result builtin_socket_send_all_bytes(Result args[], int n_args);
Result builtin_socket_recv(Result args[], int n_args);
Result builtin_socket_recv_bytes(Result args[], int n_args);
Result builtin_socket_recv_exact_bytes(Result args[], int n_args);
Result builtin_socket_recv_until_bytes(Result args[], int n_args);
Result builtin_socket_sendto(Result args[], int n_args);
Result builtin_socket_sendto_bytes(Result args[], int n_args);
Result builtin_socket_recvfrom(Result args[], int n_args);
Result builtin_socket_recvfrom_bytes(Result args[], int n_args);
Result builtin_socket_close(Result args[], int n_args);
Result builtin_socket_set_nonblocking(Result args[], int n_args);
Result builtin_socket_select(Result args[], int n_args);
Result builtin_socket_getsockname(Result args[], int n_args);
Result builtin_socket_getpeername(Result args[], int n_args);
Result builtin_socket_set_reuseaddr(Result args[], int n_args);
Result builtin_socket_set_keepalive(Result args[], int n_args);
Result builtin_socket_set_nodelay(Result args[], int n_args);
Result builtin_socket_set_broadcast(Result args[], int n_args);
Result builtin_socket_multicast_join(Result args[], int n_args);
Result builtin_socket_multicast_leave(Result args[], int n_args);
Result builtin_socket_last_error(Result args[], int n_args);
Result builtin_socket_set_hdrincl(Result args[], int n_args);
Result builtin_socket_sendto_raw(Result args[], int n_args);
Result builtin_socket_setsockopt(Result args[], int n_args);
Result builtin_socket_getsockopt(Result args[], int n_args);
Result builtin_socket_set_timeout(Result args[], int n_args);
Result builtin_socket_shutdown(Result args[], int n_args);

Result builtin_time_now_ms(Result args[], int n_args);
Result builtin_time_monotonic_ms(Result args[], int n_args);
Result builtin_time_sleep_ms(Result args[], int n_args);
Result builtin_time_format_utc_ms(Result args[], int n_args);
Result builtin_time_format_local_ms(Result args[], int n_args);

TipoExacto string_a_tipo_exacto(const char* s);
const char* tipo_exacto_a_string(TipoExacto te);
void aplicar_limites_tipo(Result* r, TipoExacto te);
void macro_insert(const char* nombre, const char* reemplazo, TipoExacto te);
Macro* macro_lookup(const char* nombre);

void itsuki_fmt(const char* filepath);

void lsp_start();
extern bool lsp_mode;
extern jmp_buf lsp_jmp;
extern char lsp_error_msg[2048];

#endif
