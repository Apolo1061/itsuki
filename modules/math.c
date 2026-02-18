#include "itsuki_ext.h"
#include <math.h>
#include <stdlib.h>

static const ItsukiApi* g_api = NULL;

static int is_numeric(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

static Result fail(TipoError tipo, const char* msg) {
    if (g_api && g_api->raise) g_api->raise(tipo, msg);
    return g_api ? g_api->make_null() : (Result){0};
}

static int as_int_nonneg(Result r, long long* out) {
    double v;
    double vf;
    if (!is_numeric(r)) return 0;
    v = r.n;
    if (v < 0) return 0;
    vf = floor(v);
    if (fabs(v - vf) > 1e-9) return 0;
    *out = (long long)vf;
    return 1;
}

static int as_int_any(Result r, long long* out) {
    double v;
    double vf;
    if (!is_numeric(r)) return 0;
    v = r.n;
    vf = floor(v);
    if (fabs(v - vf) > 1e-9) return 0;
    *out = (long long)vf;
    return 1;
}

static Result fn_clamp(Result args[], int n_args) {
    double x;
    double lo;
    double hi;
    if (n_args < 3 || !is_numeric(args[0]) || !is_numeric(args[1]) || !is_numeric(args[2])) {
        return fail(ERROR_ARGUMENTO, "math.clamp(x, min, max) requiere 3 numeros");
    }
    x = args[0].n;
    lo = args[1].n;
    hi = args[2].n;
    if (lo > hi) {
        double t = lo;
        lo = hi;
        hi = t;
    }
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return g_api->make_number(x);
}

static Result fn_factorial(Result args[], int n_args) {
    long long n;
    long long i;
    double acc = 1.0;
    if (n_args < 1 || !as_int_nonneg(args[0], &n)) {
        return fail(ERROR_ARGUMENTO, "math.factorial(n) requiere entero >= 0");
    }
    if (n > 170) {
        return fail(ERROR_ARGUMENTO, "math.factorial(n): n demasiado grande (max 170)");
    }
    for (i = 2; i <= n; i++) acc *= (double)i;
    return g_api->make_number(acc);
}

static long long gcd_ll(long long a, long long b) {
    long long t;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static Result fn_mcd(Result args[], int n_args) {
    long long a;
    long long b;
    long long g;
    if (n_args < 2 || !as_int_any(args[0], &a) || !as_int_any(args[1], &b)) {
        return fail(ERROR_ARGUMENTO, "math.mcd(a, b) requiere 2 enteros");
    }
    g = gcd_ll(a, b);
    return g_api->make_number((double)g);
}

static Result fn_mcm(Result args[], int n_args) {
    long long a;
    long long b;
    long long g;
    double out;
    if (n_args < 2 || !as_int_any(args[0], &a) || !as_int_any(args[1], &b)) {
        return fail(ERROR_ARGUMENTO, "math.mcm(a, b) requiere 2 enteros");
    }
    if (a == 0 || b == 0) return g_api->make_number(0);
    g = gcd_ll(a, b);
    out = fabs((double)a / (double)g * (double)b);
    return g_api->make_number(out);
}

static Result fn_es_primo(Result args[], int n_args) {
    long long n;
    long long i;
    if (n_args < 1 || !as_int_any(args[0], &n)) {
        return fail(ERROR_ARGUMENTO, "math.es_primo(n) requiere entero");
    }
    if (n < 2) return g_api->make_bool(0);
    if (n == 2) return g_api->make_bool(1);
    if ((n % 2) == 0) return g_api->make_bool(0);
    for (i = 3; i * i <= n; i += 2) {
        if ((n % i) == 0) return g_api->make_bool(0);
    }
    return g_api->make_bool(1);
}

static Result fn_fibonacci(Result args[], int n_args) {
    long long n;
    long long i;
    unsigned long long a = 0;
    unsigned long long b = 1;
    unsigned long long c;
    if (n_args < 1 || !as_int_nonneg(args[0], &n)) {
        return fail(ERROR_ARGUMENTO, "math.fibonacci(n) requiere entero >= 0");
    }
    if (n > 93) {
        return fail(ERROR_ARGUMENTO, "math.fibonacci(n): n demasiado grande (max 93)");
    }
    if (n == 0) return g_api->make_number(0);
    if (n == 1) return g_api->make_number(1);
    for (i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return g_api->make_number((double)b);
}

static int read_numeric_array(Result arr, double** out_vals, int* out_n, const char* fn_name) {
    int i;
    double* vals;
    if (arr.tipo != TIPO_ARRAY || !arr.a) {
        fail(ERROR_ARGUMENTO, fn_name);
        return 0;
    }
    if (arr.a->tamano <= 0) {
        fail(ERROR_ARGUMENTO, "array vacio");
        return 0;
    }
    vals = (double*)malloc(sizeof(double) * (size_t)arr.a->tamano);
    if (!vals) {
        fail(ERROR_SISTEMA, "sin memoria");
        return 0;
    }
    for (i = 0; i < arr.a->tamano; i++) {
        Result v = arr.a->elementos[i];
        if (!is_numeric(v)) {
            free(vals);
            fail(ERROR_TIPO, "array debe contener solo numeros");
            return 0;
        }
        vals[i] = v.n;
    }
    *out_vals = vals;
    *out_n = arr.a->tamano;
    return 1;
}

static Result fn_suma(Result args[], int n_args) {
    int i;
    double total = 0.0;
    if (n_args < 1 || args[0].tipo != TIPO_ARRAY || !args[0].a) {
        return fail(ERROR_ARGUMENTO, "math.suma(array) requiere array");
    }
    for (i = 0; i < args[0].a->tamano; i++) {
        Result v = args[0].a->elementos[i];
        if (!is_numeric(v)) return fail(ERROR_TIPO, "array debe contener solo numeros");
        total += v.n;
    }
    return g_api->make_number(total);
}

static Result fn_promedio(Result args[], int n_args) {
    int i;
    double total = 0.0;
    int n;
    if (n_args < 1 || args[0].tipo != TIPO_ARRAY || !args[0].a || args[0].a->tamano <= 0) {
        return fail(ERROR_ARGUMENTO, "math.promedio(array) requiere array no vacio");
    }
    n = args[0].a->tamano;
    for (i = 0; i < n; i++) {
        Result v = args[0].a->elementos[i];
        if (!is_numeric(v)) return fail(ERROR_TIPO, "array debe contener solo numeros");
        total += v.n;
    }
    return g_api->make_number(total / (double)n);
}

static Result fn_min_lista(Result args[], int n_args) {
    int i;
    double m;
    if (n_args < 1 || args[0].tipo != TIPO_ARRAY || !args[0].a || args[0].a->tamano <= 0) {
        return fail(ERROR_ARGUMENTO, "math.min_lista(array) requiere array no vacio");
    }
    if (!is_numeric(args[0].a->elementos[0])) return fail(ERROR_TIPO, "array debe contener solo numeros");
    m = args[0].a->elementos[0].n;
    for (i = 1; i < args[0].a->tamano; i++) {
        Result v = args[0].a->elementos[i];
        if (!is_numeric(v)) return fail(ERROR_TIPO, "array debe contener solo numeros");
        if (v.n < m) m = v.n;
    }
    return g_api->make_number(m);
}

static Result fn_max_lista(Result args[], int n_args) {
    int i;
    double m;
    if (n_args < 1 || args[0].tipo != TIPO_ARRAY || !args[0].a || args[0].a->tamano <= 0) {
        return fail(ERROR_ARGUMENTO, "math.max_lista(array) requiere array no vacio");
    }
    if (!is_numeric(args[0].a->elementos[0])) return fail(ERROR_TIPO, "array debe contener solo numeros");
    m = args[0].a->elementos[0].n;
    for (i = 1; i < args[0].a->tamano; i++) {
        Result v = args[0].a->elementos[i];
        if (!is_numeric(v)) return fail(ERROR_TIPO, "array debe contener solo numeros");
        if (v.n > m) m = v.n;
    }
    return g_api->make_number(m);
}

static int cmp_double_asc(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static Result fn_mediana(Result args[], int n_args) {
    double* vals = NULL;
    int n = 0;
    double out;
    if (n_args < 1) return fail(ERROR_ARGUMENTO, "math.mediana(array) requiere array");
    if (!read_numeric_array(args[0], &vals, &n, "math.mediana(array) requiere array no vacio")) {
        return g_api->make_null();
    }
    qsort(vals, (size_t)n, sizeof(double), cmp_double_asc);
    if ((n % 2) == 1) out = vals[n / 2];
    else out = (vals[(n / 2) - 1] + vals[n / 2]) / 2.0;
    free(vals);
    return g_api->make_number(out);
}

static double sqrt_newton(double x) {
    int i;
    double g;
    if (x <= 0) return 0;
    g = (x < 1.0) ? 1.0 : x;
    for (i = 0; i < 24; i++) g = 0.5 * (g + x / g);
    return g;
}

static Result fn_raiz(Result args[], int n_args) {
    double x;
    if (n_args < 1 || !is_numeric(args[0])) return fail(ERROR_ARGUMENTO, "math.raiz(x) requiere numero");
    x = args[0].n;
    if (x < 0) return fail(ERROR_ARGUMENTO, "math.raiz(x): x no puede ser negativo");
    return g_api->make_number(sqrt_newton(x));
}

static Result fn_varianza(Result args[], int n_args) {
    int i;
    int n;
    double sum = 0.0;
    double mean;
    double acc = 0.0;
    if (n_args < 1 || args[0].tipo != TIPO_ARRAY || !args[0].a || args[0].a->tamano <= 0) {
        return fail(ERROR_ARGUMENTO, "math.varianza(array) requiere array no vacio");
    }
    n = args[0].a->tamano;
    for (i = 0; i < n; i++) {
        Result v = args[0].a->elementos[i];
        if (!is_numeric(v)) return fail(ERROR_TIPO, "array debe contener solo numeros");
        sum += v.n;
    }
    mean = sum / (double)n;
    for (i = 0; i < n; i++) {
        double d = args[0].a->elementos[i].n - mean;
        acc += d * d;
    }
    return g_api->make_number(acc / (double)n);
}

static Result fn_desviacion(Result args[], int n_args) {
    Result v = fn_varianza(args, n_args);
    if (v.tipo != TIPO_NUMERO) return v;
    return g_api->make_number(sqrt_newton(v.n));
}

static Result fn_distancia2d(Result args[], int n_args) {
    double x1, y1, x2, y2;
    double dx, dy;
    if (n_args < 4 || !is_numeric(args[0]) || !is_numeric(args[1]) || !is_numeric(args[2]) || !is_numeric(args[3])) {
        return fail(ERROR_ARGUMENTO, "math.distancia2d(x1,y1,x2,y2) requiere 4 numeros");
    }
    x1 = args[0].n; y1 = args[1].n; x2 = args[2].n; y2 = args[3].n;
    dx = x2 - x1;
    dy = y2 - y1;
    return g_api->make_number(sqrt_newton(dx * dx + dy * dy));
}

static Result fn_rango(Result args[], int n_args) {
    double inicio;
    double fin;
    double paso = 1.0;
    Result out;
    if (n_args < 2 || !is_numeric(args[0]) || !is_numeric(args[1])) {
        return fail(ERROR_ARGUMENTO, "math.rango(inicio, fin, [paso]) requiere numeros");
    }
    if (n_args >= 3) {
        if (!is_numeric(args[2])) return fail(ERROR_ARGUMENTO, "math.rango: paso debe ser numero");
        paso = args[2].n;
    }
    if (paso == 0.0) return fail(ERROR_ARGUMENTO, "math.rango: paso no puede ser 0");

    inicio = args[0].n;
    fin = args[1].n;
    out = g_api->make_array();
    if (paso > 0) {
        double x = inicio;
        while (x < fin) {
            if (!g_api->array_push(out, g_api->make_number(x))) return fail(ERROR_SISTEMA, "math.rango: error push");
            x += paso;
        }
    } else {
        double x = inicio;
        while (x > fin) {
            if (!g_api->array_push(out, g_api->make_number(x))) return fail(ERROR_SISTEMA, "math.rango: error push");
            x += paso;
        }
    }
    return out;
}

#define EXPORT_FN(name, fn) do { if (!api->export_native(module, name, fn)) return 0; } while (0)

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;
    g_api = api;

    if (!api->export_const(module, "PI", api->make_number(3.14159265358979323846))) return 0;
    if (!api->export_const(module, "TAU", api->make_number(6.28318530717958647692))) return 0;
    if (!api->export_const(module, "E", api->make_number(2.71828182845904523536))) return 0;

    EXPORT_FN("clamp", fn_clamp);
    EXPORT_FN("factorial", fn_factorial);
    EXPORT_FN("mcd", fn_mcd);
    EXPORT_FN("mcm", fn_mcm);
    EXPORT_FN("es_primo", fn_es_primo);
    EXPORT_FN("fibonacci", fn_fibonacci);
    EXPORT_FN("suma", fn_suma);
    EXPORT_FN("promedio", fn_promedio);
    EXPORT_FN("min_lista", fn_min_lista);
    EXPORT_FN("max_lista", fn_max_lista);
    EXPORT_FN("mediana", fn_mediana);
    EXPORT_FN("raiz", fn_raiz);
    EXPORT_FN("varianza", fn_varianza);
    EXPORT_FN("desviacion", fn_desviacion);
    EXPORT_FN("distancia2d", fn_distancia2d);
    EXPORT_FN("rango", fn_rango);

    return 1;
}
