#include "itsuki.h"
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

Result result_to_string_gc(Result r) {
    char buffer[1024];
    if(r.tipo == TIPO_CADENA && r.s) return gc_new_string(r.s);
    if(r.tipo == TIPO_NUMERO) { sprintf(buffer, "%g", r.n); return gc_new_string(buffer); }
    if(r.tipo == TIPO_NULO) return gc_new_string("nulo");
    if(r.tipo == TIPO_BOOL) return gc_new_string(r.n ? "verdadero" : "falso");
    if(r.tipo == TIPO_ARRAY) return gc_new_string("[Array]");
    if(r.tipo == TIPO_MAP) return gc_new_string("{Map}");
    if(r.tipo == TIPO_SOCKET) {
        sprintf(buffer, "[Socket:%d]", (int)((ObjSocket*)r.obj)->handle);
        return gc_new_string(buffer);
    }
    return gc_new_string("");
}

Result builtin_tipo_de(Result args[], int n_args) {
    if(n_args < 1) return (Result){.tipo = TIPO_NULO};
    Result c = args[0];
    
    if (c.tipo_ex != TEX_AUTO) {
        return gc_new_string(tipo_exacto_a_string(c.tipo_ex));
    } else {
        if(c.tipo == TIPO_NUMERO) return gc_new_string("numero");
        else if(c.tipo == TIPO_CADENA) return gc_new_string("cadena");
        else if(c.tipo == TIPO_ARRAY) return gc_new_string("array");
        else if(c.tipo == TIPO_MAP) return gc_new_string("mapa");
        else if(c.tipo == TIPO_BOOL) return gc_new_string("booleano");
        else if(c.tipo == TIPO_INSTANCIA) return gc_new_string("instancia");
        else if(c.tipo == TIPO_SOCKET) return gc_new_string("socket");
        else return gc_new_string("nulo");
    }
}

Result builtin_string(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "string() requiere 1 argumento");
    return result_to_string_gc(args[0]);
}

Result builtin_largo(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "largo() requiere 1 argumento");
    if(args[0].tipo == TIPO_CADENA) return (Result){.n = (double)strlen(args[0].s), .tipo = TIPO_NUMERO};
    if(args[0].tipo == TIPO_ARRAY) return (Result){.n = (double)args[0].a->tamano, .tipo = TIPO_NUMERO};
    return (Result){.n = 0, .tipo = TIPO_NUMERO};
}

Result builtin_int(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "int() requiere 1 argumento");
    Result r = {0};
    r.tipo = TIPO_NUMERO;
    if(args[0].tipo == TIPO_NUMERO) r.n = (int)args[0].n;
    else if(args[0].tipo == TIPO_CADENA) r.n = atoi(args[0].s);
    else if(args[0].tipo == TIPO_BOOL) r.n = args[0].n;
    else lanzar_error(ERROR_TIPO, "No se puede convertir a int");
    return r;
}

Result builtin_decimal(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "decimal() requiere 1 argumento");
    Result r = {0};
    r.tipo = TIPO_NUMERO;
    if(args[0].tipo == TIPO_NUMERO) r.n = args[0].n;
    else if(args[0].tipo == TIPO_CADENA) r.n = atof(args[0].s);
    else if(args[0].tipo == TIPO_BOOL) r.n = args[0].n;
    else lanzar_error(ERROR_TIPO, "No se puede convertir a decimal");
    return r;
}

Result builtin_booleano(Result args[], int n_args) {
    if(n_args < 1) return (Result){.tipo = TIPO_NULO};
    Result c = args[0];
    Result r = {0};
    r.tipo = TIPO_BOOL;
    if(c.tipo == TIPO_NUMERO) r.n = (c.n != 0) ? 1 : 0;
    else if(c.tipo == TIPO_CADENA) r.n = (c.s && strlen(c.s) > 0) ? 1 : 0;
    else if(c.tipo == TIPO_NULO) r.n = 0;
    else r.n = 1;
    return r;
}

Result builtin_mayusculas(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_CADENA) lanzar_error(ERROR_TIPO, "mayusculas() requiere una cadena");
    char* upper = strdup(args[0].s);
    for(int i=0; upper[i]; i++) upper[i] = toupper(upper[i]);
    Result res = gc_new_string(upper);
    free(upper);
    return res;
}

Result builtin_minusculas(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_CADENA) lanzar_error(ERROR_TIPO, "minusculas() requiere una cadena");
    char* lower = strdup(args[0].s);
    for(int i=0; lower[i]; i++) lower[i] = tolower(lower[i]);
    Result res = gc_new_string(lower);
    free(lower);
    return res;
}

Result builtin_subcadena(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_NUMERO) lanzar_error(ERROR_ARGUMENTO, "subcadena() requiere (cadena, numero, [numero])");
    int start = (int)args[1].n;
    int end = (n_args > 2 && args[2].tipo == TIPO_NUMERO) ? (int)args[2].n : strlen(args[0].s);
    
    int len = strlen(args[0].s);
    if(start < 0) start = 0; 
    if(start > len) start = len;
    if(end < start) end = start; 
    if(end > len) end = len;
    
    int sub_len = end - start;
    char* sub = malloc(sub_len + 1);
    strncpy(sub, args[0].s + start, sub_len);
    sub[sub_len] = 0;
    
    Result res = gc_new_string(sub);
    free(sub);
    return res;
}

Result builtin_abs(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_NUMERO) return (Result){.tipo = TIPO_NULO};
    return (Result){.n = fabs(args[0].n), .tipo = TIPO_NUMERO};
}

Result builtin_potencia(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_NUMERO || args[1].tipo != TIPO_NUMERO) return (Result){.tipo = TIPO_NULO};
    return (Result){.n = pow(args[0].n, args[1].n), .tipo = TIPO_NUMERO};
}

#ifdef _WIN32
    #include <windows.h>
#endif

Result builtin_dormir(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_NUMERO) return (Result){.tipo = TIPO_NULO};
    #ifdef _WIN32
        Sleep((DWORD)(args[0].n * 1000));
    #else
        usleep((useconds_t)(args[0].n * 1000000));
    #endif
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_input(Result args[], int n_args) {
    if(n_args > 0) {
        Result s = result_to_string_gc(args[0]);
        printf("%s", s.s);
        fflush(stdout);
    }
    char buffer[1024];
    if(!fgets(buffer, 1024, stdin)) return (Result){.tipo = TIPO_NULO};
    buffer[strcspn(buffer, "\n")] = 0;
    return gc_new_string(buffer);
}

Result builtin_salir(Result args[], int n_args) {
    int code = (n_args > 0 && args[0].tipo == TIPO_NUMERO) ? (int)args[0].n : 0;
    exit(code);
    return (Result){.tipo = TIPO_NULO}; // Unreachable
}

Result builtin_system(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_CADENA) return (Result){.n = -1, .tipo = TIPO_NUMERO};
    int ret = system(args[0].s);
    return (Result){.n = (double)ret, .tipo = TIPO_NUMERO};
}

Result builtin_args(Result args[], int n_args) {
    if (!vm.cli_args) return gc_new_array(); // Should be initialized by CLI
    Result r;
    r.tipo = TIPO_ARRAY;
    r.a = vm.cli_args;
    r.obj = (Obj*)vm.cli_args;
    return r;
}

Result builtin_platform(Result args[], int n_args) {
    #ifdef _WIN32
    return gc_new_string("windows");
    #else
    return gc_new_string("linux");
    #endif
}

Result builtin_leer_archivo(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_CADENA) lanzar_error(ERROR_ARGUMENTO, "leer_archivo() requiere nombre de archivo");
    FILE* f = fopen(args[0].s, "rb");
    if(!f) lanzar_error(ERROR_ARCHIVO, "No se pudo abrir el archivo '%s'", args[0].s);
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(length + 1);
    if (fread(buffer, 1, length, f) != (size_t)length) {
        free(buffer);
        fclose(f);
        lanzar_error(ERROR_ARCHIVO, "No se pudo leer el archivo '%s'", args[0].s);
    }
    buffer[length] = '\0';
    fclose(f);
    
    Result res = gc_new_string(buffer);
    free(buffer);
    return res;
}

Result builtin_escribir_archivo(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA) return (Result){.tipo = TIPO_BOOL};
    FILE* f = fopen(args[0].s, "wb");
    if(!f) return (Result){.tipo = TIPO_BOOL}; // false
    
    fputs(args[1].s, f);
    fclose(f);
    return (Result){.n = 1, .tipo = TIPO_BOOL}; // true
}

Result builtin_existe_archivo(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_CADENA) return (Result){.tipo = TIPO_BOOL};
    FILE* f = fopen(args[0].s, "r");
    if(f) { fclose(f); return (Result){.n = 1, .tipo = TIPO_BOOL}; }
    return (Result){.tipo = TIPO_BOOL};
}

Result builtin_split(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA) 
        lanzar_error(ERROR_ARGUMENTO, "split(cadena, separador) requiere 2 strings");
    
    char* str = args[0].s;
    char* sep = args[1].s;
    Array* res_arr = array_crear(10);
    Result r = {0};
    r.tipo = TIPO_ARRAY;
    r.a = res_arr;
    r.obj = (Obj*)res_arr;
    
    char* copia = strdup(str);
    char* token = strtok(copia, sep);
    while(token) {
        array_agregar(res_arr, gc_new_string(token));
        token = strtok(NULL, sep);
    }
    free(copia);
    return r;
}

Result builtin_liberar(Result args[], int n_args) {
    if (n_args != 1) return (Result){.tipo = TIPO_NULO};
    
    if (args[0].obj) {
        liberar_objeto(args[0].obj);
    } else if (args[0].tipo == TIPO_CADENA && args[0].s) {
        free(args[0].s); 
    }
    
    return (Result){.tipo = TIPO_NULO};
}

Result builtin_join(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_CADENA)
        lanzar_error(ERROR_ARGUMENTO, "join(array, separador) requiere array y string");
    
    Array* arr = args[0].a;
    char* sep = args[1].s;
    char buffer[4096] = "";
    
    for(int i = 0; i < arr->tamano; i++) {
        Result res_str = result_to_string_gc(arr->elementos[i]);
        strcat(buffer, res_str.s);
        if(i < arr->tamano - 1) strcat(buffer, sep);
    }
    
    return gc_new_string(buffer);
}

Result builtin_replace(Result args[], int n_args) {
    if(n_args < 3 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA || args[2].tipo != TIPO_CADENA)
        lanzar_error(ERROR_ARGUMENTO, "replace(cadena, viejo, nuevo) requiere 3 strings");
    
    char* str = args[0].s;
    char* old = args[1].s;
    char* new = args[2].s;
    char buffer[4096] = "";
    
    char* pos = strstr(str, old);
    if(!pos) return gc_new_string(str);
    
    strncpy(buffer, str, pos - str);
    buffer[pos - str] = '\0';
    strcat(buffer, new);
    strcat(buffer, pos + strlen(old));
    
    return gc_new_string(buffer);
}

Result builtin_contiene(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA)
        lanzar_error(ERROR_ARGUMENTO, "contiene(cadena, subcadena) requiere 2 strings");
    
    int contiene = strstr(args[0].s, args[1].s) != NULL;
    return (Result){.n = contiene ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

Result builtin_empieza_con(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA)
        lanzar_error(ERROR_ARGUMENTO, "empieza_con(cadena, prefijo) requiere 2 strings");
    
    int resultado = strncmp(args[0].s, args[1].s, strlen(args[1].s)) == 0;
    return (Result){.n = resultado ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

Result builtin_termina_con(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_CADENA)
        lanzar_error(ERROR_ARGUMENTO, "termina_con(cadena, sufijo) requiere 2 strings");
    
    char* str = args[0].s;
    char* suffix = args[1].s;
    int len_str = strlen(str);
    int len_suf = strlen(suffix);
    
    if(len_suf > len_str) return (Result){.tipo = TIPO_BOOL};
    
    int resultado = strcmp(str + len_str - len_suf, suffix) == 0;
    return (Result){.n = resultado ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

Result builtin_repetir(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_CADENA || args[1].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "repetir(cadena, veces) requiere string y numero");
    
    char buffer[4096] = "";
    int veces = (int)args[1].n;
    for(int i = 0; i < veces && i < 100; i++) {
        strcat(buffer, args[0].s);
    }
    return gc_new_string(buffer);
}

Result builtin_agregar(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY)
        lanzar_error(ERROR_ARGUMENTO, "agregar(array, elemento) requiere array");
    
    array_agregar(args[0].a, args[1]);
    return args[0];
}

Result builtin_quitar(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "quitar(array, indice) requiere array e indice");
    
    Array* arr = args[0].a;
    int idx = (int)args[1].n;
    
    if(idx < 0 || idx >= arr->tamano) 
        lanzar_error(ERROR_INDICE, "Índice fuera de rango");
    
    for(int i = idx; i < arr->tamano - 1; i++) {
        arr->elementos[i] = arr->elementos[i + 1];
    }
    arr->tamano--;
    
    return args[0];
}

Result builtin_reverso(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_ARRAY)
        lanzar_error(ERROR_ARGUMENTO, "reverso(array) requiere array");
    
    Array* arr = args[0].a;
    for(int i = 0; i < arr->tamano / 2; i++) {
        Result temp = arr->elementos[i];
        arr->elementos[i] = arr->elementos[arr->tamano - 1 - i];
        arr->elementos[arr->tamano - 1 - i] = temp;
    }
    return args[0];
}

Result builtin_ordenar(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_ARRAY)
        lanzar_error(ERROR_ARGUMENTO, "ordenar(array) requiere array");
    
    Array* arr = args[0].a;
    for(int i = 0; i < arr->tamano - 1; i++) {
        for(int j = 0; j < arr->tamano - i - 1; j++) {
            if(arr->elementos[j].tipo == TIPO_NUMERO && arr->elementos[j+1].tipo == TIPO_NUMERO) {
                if(arr->elementos[j].n > arr->elementos[j+1].n) {
                    Result temp = arr->elementos[j];
                    arr->elementos[j] = arr->elementos[j+1];
                    arr->elementos[j+1] = temp;
                }
            }
        }
    }
    return args[0];
}

Result builtin_min(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "min() requiere al menos 1 argumento");
    
    double minimo = args[0].tipo == TIPO_NUMERO ? args[0].n : 0;
    for(int i = 1; i < n_args; i++) {
        if(args[i].tipo == TIPO_NUMERO && args[i].n < minimo) {
            minimo = args[i].n;
        }
    }
    return (Result){.n = minimo, .tipo = TIPO_NUMERO};
}

Result builtin_max(Result args[], int n_args) {
    if(n_args < 1) lanzar_error(ERROR_ARGUMENTO, "max() requiere al menos 1 argumento");
    
    double maximo = args[0].tipo == TIPO_NUMERO ? args[0].n : 0;
    for(int i = 1; i < n_args; i++) {
        if(args[i].tipo == TIPO_NUMERO && args[i].n > maximo) {
            maximo = args[i].n;
        }
    }
    return (Result){.n = maximo, .tipo = TIPO_NUMERO};
}

Result builtin_redondear(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "redondear(numero) requiere numero");
    
    return (Result){.n = round(args[0].n), .tipo = TIPO_NUMERO};
}

Result builtin_piso(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "piso(numero) requiere numero");
    
    return (Result){.n = floor(args[0].n), .tipo = TIPO_NUMERO};
}

Result builtin_techo(Result args[], int n_args) {
    if(n_args < 1 || args[0].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "techo(numero) requiere numero");
    
    return (Result){.n = ceil(args[0].n), .tipo = TIPO_NUMERO};
}

Result builtin_aleatorio(Result args[], int n_args) {
    return (Result){.n = (double)rand() / RAND_MAX, .tipo = TIPO_NUMERO};
}

Result builtin_aleatorio_rango(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_NUMERO || args[1].tipo != TIPO_NUMERO)
        lanzar_error(ERROR_ARGUMENTO, "aleatorio_rango(min, max) requiere 2 numeros");
    
    double min = args[0].n;
    double max = args[1].n;
    double aleatorio = min + ((double)rand() / RAND_MAX) * (max - min);
    return (Result){.n = aleatorio, .tipo = TIPO_NUMERO};
}

Result builtin_es_numero(Result args[], int n_args) {
    if(n_args < 1) return (Result){.tipo = TIPO_BOOL};
    return (Result){.n = args[0].tipo == TIPO_NUMERO ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

Result builtin_es_cadena(Result args[], int n_args) {
    if(n_args < 1) return (Result){.tipo = TIPO_BOOL};
    return (Result){.n = args[0].tipo == TIPO_CADENA ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

Result builtin_es_array(Result args[], int n_args) {
    if(n_args < 1) return (Result){.tipo = TIPO_BOOL};
    return (Result){.n = args[0].tipo == TIPO_ARRAY ? 1.0 : 0.0, .tipo = TIPO_BOOL};
}

extern Result llamar_funcion_usuario(int f_idx, Result args[], int n_args);

Result builtin_filtrar(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_FUNCION)
        lanzar_error(ERROR_ARGUMENTO, "filtrar(array, funcion) requiere array y funcion");
    
    Array* arr = args[0].a;
    int func_idx = args[1].func_index;
    Array* res_arr = array_crear(10);
    Result resultado = {0};
    resultado.tipo = TIPO_ARRAY;
    resultado.a = res_arr;
    resultado.obj = (Obj*)res_arr;
    
    for(int i = 0; i < arr->tamano; i++) {
        Result arg_array[1] = {arr->elementos[i]};
        Result test = llamar_funcion_usuario(func_idx, arg_array, 1);
        
        if((test.tipo == TIPO_BOOL || test.tipo == TIPO_NUMERO) && test.n != 0) {
            array_agregar(resultado.a, arr->elementos[i]);
        }
    }
    
    return resultado;
}

Result builtin_mapear(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_FUNCION)
        lanzar_error(ERROR_ARGUMENTO, "mapear(array, funcion) requiere array y funcion");
    
    Array* arr = args[0].a;
    int func_idx = args[1].func_index;
    Array* res_arr = array_crear(arr->tamano);
    Result resultado = {0};
    resultado.tipo = TIPO_ARRAY;
    resultado.a = res_arr;
    resultado.obj = (Obj*)res_arr;
    
    for(int i = 0; i < arr->tamano; i++) {
        Result arg_array[1] = {arr->elementos[i]};
        Result transformado = llamar_funcion_usuario(func_idx, arg_array, 1);
        array_agregar(resultado.a, transformado);
    }
    
    return resultado;
}

Result builtin_cada(Result args[], int n_args) {
    if(n_args < 2 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_FUNCION)
        lanzar_error(ERROR_ARGUMENTO, "cada(array, funcion) requiere array y funcion");
    
    Array* arr = args[0].a;
    int func_idx = args[1].func_index;
    
    for(int i = 0; i < arr->tamano; i++) {
        Result arg_array[1] = {arr->elementos[i]};
        llamar_funcion_usuario(func_idx, arg_array, 1);
    }
    
    Result r = {0};
    r.tipo = TIPO_NULO;
    return r;
}

Result builtin_array(Result args[], int n_args) {
    return gc_new_array();
}

Result builtin_mapa(Result args[], int n_args) {
    Result r = {0};
    r.tipo = TIPO_MAP;
    r.m = map_crear();
    r.obj = (Obj*)r.m;
    return r;
}

Result builtin_reducir(Result args[], int n_args) {
    if(n_args < 3 || args[0].tipo != TIPO_ARRAY || args[1].tipo != TIPO_FUNCION)
        lanzar_error(ERROR_ARGUMENTO, "reducir(array, funcion, inicial) requiere array, funcion y valor inicial");
    
    Array* arr = args[0].a;
    int func_idx = args[1].func_index;
    Result acumulador = args[2];
    
    for(int i = 0; i < arr->tamano; i++) {
        Result func_args[2] = {acumulador, arr->elementos[i]};
        acumulador = llamar_funcion_usuario(func_idx, func_args, 2);
    }
    
    return acumulador;
}

bool es_funcion_builtin(const char* nombre) {
    if (!nombre) return false;
    if (es_funcion_nativa_proxy(nombre)) return true;
    if (strncmp(nombre, "socket_", 7) == 0 || strncmp(nombre, "_socket_", 8) == 0) return true;
    if (strncmp(nombre, "time_", 5) == 0 || strncmp(nombre, "_time_", 6) == 0) return true;
    
    if(!strcmp(nombre, "tipo_de")) return true;
    if(!strcmp(nombre, "string")) return true;
    if(!strcmp(nombre, "largo")) return true;
    if(!strcmp(nombre, "int")) return true;
    if(!strcmp(nombre, "input")) return true;
    if(!strcmp(nombre, "decimal")) return true;
    if(!strcmp(nombre, "booleano") || !strcmp(nombre, "bool")) return true;
    if(!strcmp(nombre, "mayusculas")) return true;
    if(!strcmp(nombre, "minusculas")) return true;
    if(!strcmp(nombre, "subcadena")) return true;
    if(!strcmp(nombre, "abs")) return true;
    if(!strcmp(nombre, "potencia")) return true;
    if(!strcmp(nombre, "split")) return true;
    if(!strcmp(nombre, "join")) return true;
    if(!strcmp(nombre, "replace")) return true;
    if(!strcmp(nombre, "contiene")) return true;
    if(!strcmp(nombre, "empieza_con")) return true;
    if(!strcmp(nombre, "termina_con")) return true;
    if(!strcmp(nombre, "repetir")) return true;
    if(!strcmp(nombre, "agregar")) return true;
    if(!strcmp(nombre, "quitar")) return true;
    if(!strcmp(nombre, "reverso")) return true;
    if(!strcmp(nombre, "ordenar")) return true;
    if(!strcmp(nombre, "min")) return true;
    if(!strcmp(nombre, "max")) return true;
    if(!strcmp(nombre, "redondear")) return true;
    if(!strcmp(nombre, "piso")) return true;
    if(!strcmp(nombre, "techo")) return true;
    if(!strcmp(nombre, "aleatorio")) return true;
    if(!strcmp(nombre, "aleatorio_rango")) return true;
    if(!strcmp(nombre, "es_numero")) return true;
    if(!strcmp(nombre, "es_cadena")) return true;
    if(!strcmp(nombre, "es_array")) return true;
    if(!strcmp(nombre, "filtrar")) return true;
    if(!strcmp(nombre, "mapear")) return true;
    if(!strcmp(nombre, "cada")) return true;
    if(!strcmp(nombre, "reducir")) return true;
    if(!strcmp(nombre, "largo")) return true;
    if(!strcmp(nombre, "dormir")) return true;
    if(!strcmp(nombre, "salir")) return true;
    if(!strcmp(nombre, "leer_archivo")) return true;
    if(!strcmp(nombre, "escribir_archivo")) return true;
    if(!strcmp(nombre, "existe_archivo")) return true;
    if(!strcmp(nombre, "liberar")) return true;
    if(!strcmp(nombre, "conseguir_mem")) return true;
    if(!strcmp(nombre, "system")) return true;
    if(!strcmp(nombre, "cli_args")) return true;
    if(!strcmp(nombre, "platform")) return true;

    return false;
}

Result builtin_conseguir_mem(Result args[], int n_args) {
    if (n_args != 1) return (Result){.tipo = TIPO_NULO}; // Expect size
    int size = (int)args[0].n;
    if (size <= 0) return (Result){.tipo = TIPO_NULO};
    
    char* mem = malloc(size + 1);
    if (!mem) return (Result){.tipo = TIPO_NULO};
    memset(mem, 0, size + 1);
    
    return (Result){.tipo = TIPO_CADENA, .s = mem};
}

Result ejecutar_builtin(const char* nombre, Result args[], int n_args) {
    if (es_funcion_nativa_proxy(nombre)) return ejecutar_nativa_proxy(nombre, args, n_args);

    if(!strcmp(nombre, "tipo_de")) return builtin_tipo_de(args, n_args);
    if(!strcmp(nombre, "conseguir_mem")) return builtin_conseguir_mem(args, n_args);
    if(!strcmp(nombre, "string")) return builtin_string(args, n_args);
    if(!strcmp(nombre, "largo")) return builtin_largo(args, n_args);
    if(!strcmp(nombre, "int")) return builtin_int(args, n_args);
    if(!strcmp(nombre, "input")) return builtin_input(args, n_args);
    if(!strcmp(nombre, "decimal")) return builtin_decimal(args, n_args);
    if(!strcmp(nombre, "booleano") || !strcmp(nombre, "bool")) return builtin_booleano(args, n_args);
    
    if(!strcmp(nombre, "mayusculas")) return builtin_mayusculas(args, n_args);
    if(!strcmp(nombre, "minusculas")) return builtin_minusculas(args, n_args);
    if(!strcmp(nombre, "subcadena")) return builtin_subcadena(args, n_args);
    if(!strcmp(nombre, "abs")) return builtin_abs(args, n_args);
    if(!strcmp(nombre, "potencia")) return builtin_potencia(args, n_args);
    
    if(!strcmp(nombre, "split")) return builtin_split(args, n_args);
    if(!strcmp(nombre, "join")) return builtin_join(args, n_args);
    if(!strcmp(nombre, "replace")) return builtin_replace(args, n_args);
    if(!strcmp(nombre, "contiene")) return builtin_contiene(args, n_args);
    if(!strcmp(nombre, "empieza_con")) return builtin_empieza_con(args, n_args);
    if(!strcmp(nombre, "termina_con")) return builtin_termina_con(args, n_args);
    if(!strcmp(nombre, "repetir")) return builtin_repetir(args, n_args);
    // Arrays avanzados
    if(!strcmp(nombre, "agregar")) return builtin_agregar(args, n_args);
    if(!strcmp(nombre, "quitar")) return builtin_quitar(args, n_args);
    if(!strcmp(nombre, "reverso")) return builtin_reverso(args, n_args);
    
    // Matemáticas
    if(!strcmp(nombre, "min")) return builtin_min(args, n_args);
    if(!strcmp(nombre, "max")) return builtin_max(args, n_args);
    if(!strcmp(nombre, "redondear")) return builtin_redondear(args, n_args);
    if(!strcmp(nombre, "piso")) return builtin_piso(args, n_args);
    if(!strcmp(nombre, "techo")) return builtin_techo(args, n_args);
    if(!strcmp(nombre, "aleatorio_rango")) return builtin_aleatorio_rango(args, n_args);
    
    // Tipo checking
    if(!strcmp(nombre, "es_cadena")) return builtin_es_cadena(args, n_args);
    if(!strcmp(nombre, "es_array")) return builtin_es_array(args, n_args);
    
    // Alto orden
    if(!strcmp(nombre, "filtrar")) return builtin_filtrar(args, n_args);
    
    if(!strcmp(nombre, "dormir")) return builtin_dormir(args, n_args);
    if(!strcmp(nombre, "salir")) return builtin_salir(args, n_args);
    if(!strcmp(nombre, "leer_archivo")) return builtin_leer_archivo(args, n_args);
    if(!strcmp(nombre, "escribir_archivo")) return builtin_escribir_archivo(args, n_args);
    if(!strcmp(nombre, "existe_archivo")) return builtin_existe_archivo(args, n_args);
    if(!strcmp(nombre, "system")) return builtin_system(args, n_args);
    if(!strcmp(nombre, "cli_args")) return builtin_args(args, n_args);
    if(!strcmp(nombre, "platform")) return builtin_platform(args, n_args);
    
    if(!strcmp(nombre, "array")) return builtin_array(args, n_args);
    if(!strcmp(nombre, "mapa")) return builtin_mapa(args, n_args);

    if(!strcmp(nombre, "socket_socket") || !strcmp(nombre, "_socket_socket")) return builtin_socket_socket(args, n_args);
    if(!strcmp(nombre, "socket_bind") || !strcmp(nombre, "_socket_bind")) return builtin_socket_bind(args, n_args);
    if(!strcmp(nombre, "socket_listen") || !strcmp(nombre, "_socket_listen")) return builtin_socket_listen(args, n_args);
    if(!strcmp(nombre, "socket_accept") || !strcmp(nombre, "_socket_accept")) return builtin_socket_accept(args, n_args);
    if(!strcmp(nombre, "socket_connect") || !strcmp(nombre, "_socket_connect")) return builtin_socket_connect(args, n_args);
    if(!strcmp(nombre, "socket_send") || !strcmp(nombre, "_socket_send")) return builtin_socket_send(args, n_args);
    if(!strcmp(nombre, "socket_send_bytes") || !strcmp(nombre, "_socket_send_bytes")) return builtin_socket_send_bytes(args, n_args);
    if(!strcmp(nombre, "socket_send_all_bytes") || !strcmp(nombre, "_socket_send_all_bytes")) return builtin_socket_send_all_bytes(args, n_args);
    if(!strcmp(nombre, "socket_recv") || !strcmp(nombre, "_socket_recv")) return builtin_socket_recv(args, n_args);
    if(!strcmp(nombre, "socket_recv_bytes") || !strcmp(nombre, "_socket_recv_bytes")) return builtin_socket_recv_bytes(args, n_args);
    if(!strcmp(nombre, "socket_recv_exact_bytes") || !strcmp(nombre, "_socket_recv_exact_bytes")) return builtin_socket_recv_exact_bytes(args, n_args);
    if(!strcmp(nombre, "socket_recv_until_bytes") || !strcmp(nombre, "_socket_recv_until_bytes")) return builtin_socket_recv_until_bytes(args, n_args);
    if(!strcmp(nombre, "socket_sendto") || !strcmp(nombre, "_socket_sendto")) return builtin_socket_sendto(args, n_args);
    if(!strcmp(nombre, "socket_sendto_bytes") || !strcmp(nombre, "_socket_sendto_bytes")) return builtin_socket_sendto_bytes(args, n_args);
    if(!strcmp(nombre, "socket_sendto_raw") || !strcmp(nombre, "_socket_sendto_raw")) return builtin_socket_sendto_raw(args, n_args);
    if(!strcmp(nombre, "socket_set_hdrincl") || !strcmp(nombre, "_socket_set_hdrincl")) return builtin_socket_set_hdrincl(args, n_args);
    if(!strcmp(nombre, "socket_setsockopt") || !strcmp(nombre, "_socket_setsockopt")) return builtin_socket_setsockopt(args, n_args);
    if(!strcmp(nombre, "socket_getsockopt") || !strcmp(nombre, "_socket_getsockopt")) return builtin_socket_getsockopt(args, n_args);
    if(!strcmp(nombre, "socket_set_timeout") || !strcmp(nombre, "_socket_set_timeout")) return builtin_socket_set_timeout(args, n_args);
    if(!strcmp(nombre, "socket_shutdown") || !strcmp(nombre, "_socket_shutdown")) return builtin_socket_shutdown(args, n_args);
    if(!strcmp(nombre, "socket_recvfrom") || !strcmp(nombre, "_socket_recvfrom")) return builtin_socket_recvfrom(args, n_args);
    if(!strcmp(nombre, "socket_recvfrom_bytes") || !strcmp(nombre, "_socket_recvfrom_bytes")) return builtin_socket_recvfrom_bytes(args, n_args);
    if(!strcmp(nombre, "socket_close") || !strcmp(nombre, "_socket_close")) return builtin_socket_close(args, n_args);
    if(!strcmp(nombre, "socket_set_nonblocking") || !strcmp(nombre, "_socket_set_nonblocking")) return builtin_socket_set_nonblocking(args, n_args);
    if(!strcmp(nombre, "socket_select") || !strcmp(nombre, "_socket_select")) return builtin_socket_select(args, n_args);
    if(!strcmp(nombre, "socket_getsockname") || !strcmp(nombre, "_socket_getsockname")) return builtin_socket_getsockname(args, n_args);
    if(!strcmp(nombre, "socket_getpeername") || !strcmp(nombre, "_socket_getpeername")) return builtin_socket_getpeername(args, n_args);
    if(!strcmp(nombre, "socket_set_reuseaddr") || !strcmp(nombre, "_socket_set_reuseaddr")) return builtin_socket_set_reuseaddr(args, n_args);
    if(!strcmp(nombre, "socket_set_keepalive") || !strcmp(nombre, "_socket_set_keepalive")) return builtin_socket_set_keepalive(args, n_args);
    if(!strcmp(nombre, "socket_set_nodelay") || !strcmp(nombre, "_socket_set_nodelay")) return builtin_socket_set_nodelay(args, n_args);
    if(!strcmp(nombre, "socket_set_broadcast") || !strcmp(nombre, "_socket_set_broadcast")) return builtin_socket_set_broadcast(args, n_args);
    if(!strcmp(nombre, "socket_multicast_join") || !strcmp(nombre, "_socket_multicast_join")) return builtin_socket_multicast_join(args, n_args);
    if(!strcmp(nombre, "socket_multicast_leave") || !strcmp(nombre, "_socket_multicast_leave")) return builtin_socket_multicast_leave(args, n_args);
    if(!strcmp(nombre, "socket_last_error") || !strcmp(nombre, "_socket_last_error")) return builtin_socket_last_error(args, n_args);

    if(!strcmp(nombre, "time_now_ms") || !strcmp(nombre, "_time_now_ms")) return builtin_time_now_ms(args, n_args);
    if(!strcmp(nombre, "time_monotonic_ms") || !strcmp(nombre, "_time_monotonic_ms")) return builtin_time_monotonic_ms(args, n_args);
    if(!strcmp(nombre, "time_sleep_ms") || !strcmp(nombre, "_time_sleep_ms")) return builtin_time_sleep_ms(args, n_args);
    if(!strcmp(nombre, "time_format_utc_ms") || !strcmp(nombre, "_time_format_utc_ms")) return builtin_time_format_utc_ms(args, n_args);
    if(!strcmp(nombre, "time_format_local_ms") || !strcmp(nombre, "_time_format_local_ms")) return builtin_time_format_local_ms(args, n_args);

    if(!strcmp(nombre, "liberar")) return builtin_liberar(args, n_args);
    
    Result r = {0};
    r.tipo = TIPO_NULO;
    return r;
}

void registrar_builtins() {
    const char* nombres[] = {
        "tipo_de", "string", "largo", "int", "input", "decimal", "booleano", "bool",
        "mayusculas", "minusculas", "subcadena", "abs", "potencia",
        "split", "join", "replace", "contiene", "empieza_con", "termina_con", "repetir",
        "agregar", "quitar", "reverso", "ordenar",
        "min", "max", "redondear", "piso", "techo", "aleatorio", "aleatorio_rango",
        "es_numero", "es_cadena", "es_array",
        "filtrar", "mapear", "cada", "reducir",
        "dormir", "salir", "leer_archivo", "escribir_archivo", "existe_archivo",
        "liberar", "conseguir_mem", "array", "mapa",
        "system", "cli_args", "platform",
        "socket_socket", "socket_bind", "socket_listen", "socket_accept", 
        "socket_connect", "socket_send", "socket_send_bytes", "socket_send_all_bytes", "socket_recv",
        "socket_recv_bytes", "socket_recv_exact_bytes", "socket_recv_until_bytes", "socket_close",
        "socket_sendto", "socket_sendto_bytes", "socket_sendto_raw", "socket_set_hdrincl",
        "socket_setsockopt", "socket_getsockopt", "socket_set_timeout",
        "socket_shutdown", "socket_recvfrom", "socket_recvfrom_bytes",
        "socket_set_nonblocking", "socket_select", "socket_getsockname", "socket_getpeername",
        "socket_set_reuseaddr", "socket_set_keepalive", "socket_set_nodelay", "socket_set_broadcast",
        "socket_multicast_join", "socket_multicast_leave", "socket_last_error",
        "_socket_socket", "_socket_bind", "_socket_listen", "_socket_accept", 
        "_socket_connect", "_socket_send", "_socket_send_bytes", "_socket_send_all_bytes", "_socket_recv",
        "_socket_recv_bytes", "_socket_recv_exact_bytes", "_socket_recv_until_bytes", "_socket_close",
        "_socket_sendto", "_socket_sendto_bytes", "_socket_sendto_raw", "_socket_set_hdrincl",
        "_socket_setsockopt", "_socket_getsockopt", "_socket_set_timeout",
        "_socket_shutdown",
        "_socket_recvfrom", "_socket_recvfrom_bytes",
        "_socket_set_nonblocking", "_socket_select", "_socket_getsockname", "_socket_getpeername",
        "_socket_set_reuseaddr", "_socket_set_keepalive", "_socket_set_nodelay", "_socket_set_broadcast",
        "_socket_multicast_join", "_socket_multicast_leave", "_socket_last_error"
        ,
        "time_now_ms", "time_monotonic_ms", "time_sleep_ms", "time_format_utc_ms", "time_format_local_ms",
        "_time_now_ms", "_time_monotonic_ms", "_time_sleep_ms", "_time_format_utc_ms", "_time_format_local_ms"
    };
    int total = sizeof(nombres) / sizeof(nombres[0]);
    for (int i = 0; i < total; i++) {
        int f_idx = n_f++;
        funcs[f_idx].jit_ptr = NULL;
        funcs[f_idx].jit_size = 0;
        funcs[f_idx].jit_calls = 0;
        funcs[f_idx].jit_state = 0;
        strcpy(funcs[f_idx].nombre, nombres[i]);
        funcs[f_idx].n_params = 0;
        funcs[f_idx].chunk_bytecode = NULL;
        funcs[f_idx].cuerpo_ast = NULL;
        hash_insert(&ht_funcs, nombres[i], f_idx);
    }
}
