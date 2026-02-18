#include "itsuki.h"
#include "itsuki_ext.h"
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE ItsukiExtLib;
#else
#include <dlfcn.h>
typedef void* ItsukiExtLib;
#endif

typedef struct {
    char nombre[MAX_ID_LEN];
    ItsukiNativeFn fn;
} ItsukiExtProxy;

static ItsukiExtProxy g_ext_proxies[MAX_FUNCS];
static int g_ext_proxy_count = 0;

static int itsuki_ext_endswith_icase(const char* text, const char* suffix) {
    size_t text_len;
    size_t suffix_len;
    size_t i;
    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) return 0;
    for (i = 0; i < suffix_len; i++) {
        unsigned char a = (unsigned char)text[text_len - suffix_len + i];
        unsigned char b = (unsigned char)suffix[i];
        if (tolower(a) != tolower(b)) return 0;
    }
    return 1;
}

static int itsuki_ext_tiene_extension(const char* path) {
    const char* slash;
    const char* slash2;
    const char* last_sep;
    const char* dot;
    if (!path) return 0;
    slash = strrchr(path, '/');
    slash2 = strrchr(path, '\\');
    last_sep = slash;
    if (!last_sep || (slash2 && slash2 > last_sep)) last_sep = slash2;
    dot = strrchr(path, '.');
    if (!dot) return 0;
    return (!last_sep || dot > last_sep);
}

static int itsuki_ext_archivo_existe(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void itsuki_ext_copy(char* out, size_t out_sz, const char* in) {
    if (!out || out_sz == 0) return;
    if (!in) {
        out[0] = '\0';
        return;
    }
    strncpy(out, in, out_sz - 1);
    out[out_sz - 1] = '\0';
}

static int itsuki_ext_resolver_path_nativo(const char* path_orig, char* out_path, size_t out_sz) {
    char suki_path[256];
    char native_path[256];
    if (!path_orig || !*path_orig) return 0;

    if (itsuki_ext_endswith_icase(path_orig, ".suki")) return 0;

    if (itsuki_ext_endswith_icase(path_orig, ".dll")
        || itsuki_ext_endswith_icase(path_orig, ".so")
        || itsuki_ext_endswith_icase(path_orig, ".dylib")) {
        if (!itsuki_ext_archivo_existe(path_orig)) {
            lanzar_error(ERROR_ARCHIVO, "No se encontro el modulo nativo '%s'", path_orig);
        }
        itsuki_ext_copy(out_path, out_sz, path_orig);
        return 1;
    }

    if (itsuki_ext_tiene_extension(path_orig)) {
        return 0;
    }

    snprintf(suki_path, sizeof(suki_path), "%s.suki", path_orig);
    if (itsuki_ext_archivo_existe(path_orig) || itsuki_ext_archivo_existe(suki_path)) {
        return 0;
    }

#ifdef _WIN32
    snprintf(native_path, sizeof(native_path), "%s.dll", path_orig);
    if (itsuki_ext_archivo_existe(native_path)) {
        itsuki_ext_copy(out_path, out_sz, native_path);
        return 1;
    }
#else
    snprintf(native_path, sizeof(native_path), "%s.so", path_orig);
    if (itsuki_ext_archivo_existe(native_path)) {
        itsuki_ext_copy(out_path, out_sz, native_path);
        return 1;
    }
    snprintf(native_path, sizeof(native_path), "%s.dylib", path_orig);
    if (itsuki_ext_archivo_existe(native_path)) {
        itsuki_ext_copy(out_path, out_sz, native_path);
        return 1;
    }
#endif

    return 0;
}

static ItsukiExtLib itsuki_ext_open_lib(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* itsuki_ext_get_symbol(ItsukiExtLib lib, const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress(lib, name);
#else
    return dlsym(lib, name);
#endif
}

static void itsuki_ext_close_lib(ItsukiExtLib lib) {
#ifdef _WIN32
    if (lib) FreeLibrary(lib);
#else
    if (lib) dlclose(lib);
#endif
}

static const char* itsuki_ext_last_error(void) {
#ifdef _WIN32
    static char msg[128];
    DWORD err = GetLastError();
    snprintf(msg, sizeof(msg), "codigo=%lu", (unsigned long)err);
    return msg;
#else
    const char* err = dlerror();
    return err ? err : "desconocido";
#endif
}

static uint32_t itsuki_ext_hash32(const char* s) {
    uint32_t h = 2166136261u;
    if (!s) return h;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

static void itsuki_ext_sanitize_name(char* out, size_t out_sz, const char* in) {
    size_t i;
    size_t j;
    if (!out || out_sz == 0) return;
    j = 0;
    if (!in) {
        out[0] = 'x';
        if (out_sz > 1) out[1] = '\0';
        return;
    }
    for (i = 0; in[i] && j + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c)) out[j++] = (char)tolower(c);
        else out[j++] = '_';
    }
    if (j == 0 && out_sz > 1) out[j++] = 'x';
    out[j] = '\0';
}

static void itsuki_ext_build_proxy_name(const char* module_path, const char* export_name, char out_name[MAX_ID_LEN]) {
    char safe_export[32];
    uint32_t module_hash = itsuki_ext_hash32(module_path ? module_path : "");
    itsuki_ext_sanitize_name(safe_export, sizeof(safe_export), export_name);
    snprintf(out_name, MAX_ID_LEN, "__ext_%08x_%s", (unsigned int)module_hash, safe_export);
    out_name[MAX_ID_LEN - 1] = '\0';
}

static int itsuki_ext_find_proxy(const char* nombre) {
    int i;
    if (!nombre) return -1;
    for (i = 0; i < g_ext_proxy_count; i++) {
        if (!strcmp(g_ext_proxies[i].nombre, nombre)) return i;
    }
    return -1;
}

static int itsuki_ext_registrar_funcion_vm(const char* nombre_proxy) {
    int f_idx = hash_lookup(&ht_funcs, nombre_proxy);
    if (f_idx != -1) return f_idx;
    if (n_f >= MAX_FUNCS) {
        lanzar_error(ERROR_SISTEMA, "Limite de funciones alcanzado al registrar '%s'", nombre_proxy);
    }
    f_idx = n_f++;
    funcs[f_idx].jit_ptr = NULL;
    funcs[f_idx].jit_size = 0;
    funcs[f_idx].jit_calls = 0;
    funcs[f_idx].jit_state = 0;
    strncpy(funcs[f_idx].nombre, nombre_proxy, MAX_ID_LEN - 1);
    funcs[f_idx].nombre[MAX_ID_LEN - 1] = '\0';
    funcs[f_idx].n_params = 0;
    funcs[f_idx].chunk_bytecode = NULL;
    funcs[f_idx].cuerpo_ast = NULL;
    hash_insert(&ht_funcs, funcs[f_idx].nombre, f_idx);
    return f_idx;
}

static int itsuki_ext_registrar_proxy(const char* nombre_proxy, ItsukiNativeFn fn) {
    int proxy_idx;
    if (!nombre_proxy || !fn) return -1;
    proxy_idx = itsuki_ext_find_proxy(nombre_proxy);
    if (proxy_idx == -1) {
        if (g_ext_proxy_count >= MAX_FUNCS) {
            lanzar_error(ERROR_SISTEMA, "Limite de proxies nativos alcanzado");
        }
        proxy_idx = g_ext_proxy_count++;
        strncpy(g_ext_proxies[proxy_idx].nombre, nombre_proxy, MAX_ID_LEN - 1);
        g_ext_proxies[proxy_idx].nombre[MAX_ID_LEN - 1] = '\0';
    }
    g_ext_proxies[proxy_idx].fn = fn;
    return itsuki_ext_registrar_funcion_vm(g_ext_proxies[proxy_idx].nombre);
}

static void itsuki_api_raise(TipoError tipo, const char* msg) {
    lanzar_error(tipo, "%s", msg ? msg : "Error en modulo nativo");
}

static Result itsuki_api_make_number(double n) {
    Result r = {0};
    r.tipo = TIPO_NUMERO;
    r.n = n;
    return r;
}

static Result itsuki_api_make_bool(int b) {
    Result r = {0};
    r.tipo = TIPO_BOOL;
    r.n = b ? 1.0 : 0.0;
    return r;
}

static Result itsuki_api_make_null(void) {
    Result r = {0};
    r.tipo = TIPO_NULO;
    return r;
}

static Result itsuki_api_make_string(const char* s) {
    return gc_new_string(s ? s : "");
}

static Result itsuki_api_make_array(void) {
    return gc_new_array();
}

static Result itsuki_api_make_map(void) {
    Result r = {0};
    r.tipo = TIPO_MAP;
    r.m = map_crear();
    r.obj = (Obj*)r.m;
    return r;
}

static int itsuki_api_array_push(Result array, Result v) {
    if (array.tipo != TIPO_ARRAY || !array.a) return 0;
    array_agregar(array.a, v);
    return 1;
}

static int itsuki_api_map_set(Result map, const char* key, Result v) {
    if (map.tipo != TIPO_MAP || !map.m || !key) return 0;
    map_establecer(map.m, key, v);
    return 1;
}

static int itsuki_api_export_const(ItsukiModule* module, const char* name, Result v) {
    if (!module || !module->exports || !name || !*name) return 0;
    map_establecer(module->exports, name, v);
    return 1;
}

static int itsuki_api_export_native(ItsukiModule* module, const char* name, ItsukiNativeFn fn) {
    char proxy_name[MAX_ID_LEN];
    int f_idx;
    Result fn_value = {0};
    if (!module || !module->exports || !name || !*name || !fn) return 0;
    itsuki_ext_build_proxy_name(module->path, name, proxy_name);
    f_idx = itsuki_ext_registrar_proxy(proxy_name, fn);
    if (f_idx < 0) return 0;
    fn_value.tipo = TIPO_FUNCION;
    fn_value.func_index = f_idx;
    map_establecer(module->exports, name, fn_value);
    return 1;
}

static const ItsukiApi g_itsuki_api = {
    ITSUKI_EXT_API_VERSION,
    itsuki_api_raise,
    itsuki_api_make_number,
    itsuki_api_make_bool,
    itsuki_api_make_null,
    itsuki_api_make_string,
    itsuki_api_make_array,
    itsuki_api_make_map,
    itsuki_api_array_push,
    itsuki_api_map_set,
    itsuki_api_export_const,
    itsuki_api_export_native
};

ObjModulo* itsuki_ext_cargar_modulo_nativo(const char* path_orig) {
    char native_path[256];
    ItsukiExtLib lib;
    ItsukiModuleInitFn init_fn;
    ObjModulo* mod;

    if (!itsuki_ext_resolver_path_nativo(path_orig, native_path, sizeof(native_path))) {
        return NULL;
    }

    lib = itsuki_ext_open_lib(native_path);
    if (!lib) {
        lanzar_error(ERROR_ARCHIVO, "No se pudo abrir modulo nativo '%s' (%s)", native_path, itsuki_ext_last_error());
    }

    init_fn = (ItsukiModuleInitFn)itsuki_ext_get_symbol(lib, "itsuki_module_init");
    if (!init_fn) {
        itsuki_ext_close_lib(lib);
        lanzar_error(ERROR_SISTEMA, "El modulo '%s' no exporta itsuki_module_init", native_path);
    }

    mod = (ObjModulo*)gc_alloc(sizeof(ObjModulo), OBJ_MODULO);
    strncpy(mod->path, native_path, sizeof(mod->path) - 1);
    mod->path[sizeof(mod->path) - 1] = '\0';
    mod->exports = map_crear();

    if (!init_fn(&g_itsuki_api, mod)) {
        itsuki_ext_close_lib(lib);
        lanzar_error(ERROR_EJECUCION, "Fallo al inicializar modulo nativo '%s'", native_path);
    }

    return mod;
}

bool es_funcion_nativa_proxy(const char* nombre) {
    return itsuki_ext_find_proxy(nombre) != -1;
}

Result ejecutar_nativa_proxy(const char* nombre, Result args[], int n_args) {
    int proxy_idx = itsuki_ext_find_proxy(nombre);
    if (proxy_idx == -1 || !g_ext_proxies[proxy_idx].fn) {
        Result r = {0};
        r.tipo = TIPO_NULO;
        return r;
    }
    return g_ext_proxies[proxy_idx].fn(args, n_args);
}
