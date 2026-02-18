#include "itsuki.h"
#include <math.h>
#include <time.h>

VM vm;

#define PROP_CACHE_NONE 0
#define PROP_CACHE_PROP 1
#define PROP_CACHE_METHOD 2
#define PROP_CACHE_STATIC 3

#define INVOKE_CACHE_NONE 0
#define INVOKE_CACHE_INSTANCE 1
#define INVOKE_CACHE_CLASS 2
#define INVOKE_CACHE_SUPER 3

#define ENABLE_GLOBAL_CACHE 1

void vm_init() {
    memset(&vm, 0, sizeof(vm));
    vm.pila_tope = vm.pila;
    vm.modulos_cargados = map_crear();
    vm.ht_vars_version = 1;
    vm.jit_hot_threshold = 10;
    vm.manual_memory_mode = false;
    vm.mut_proximo = MUT_VAR;

}

void vm_free() {
}

static void empujar(Result valor) {
    *vm.pila_tope = valor;
    vm.pila_tope++;
}

static Result extraer() {
    vm.pila_tope--;
    return *vm.pila_tope;
}

static Result result_desde_variable(Variable* v) {
    if (!v) return (Result){.tipo = TIPO_NULO};
    Result res = {0}; res.tipo = v->tipo; res.obj = v->obj;
    if (res.tipo == TIPO_CADENA) res.s = v->val_str;
    else if (res.tipo == TIPO_ARRAY) res.a = v->val_array;
    else if (res.tipo == TIPO_MAP) res.m = v->val_map;
    else if (res.tipo == TIPO_INSTANCIA) res.inst = v->val_inst;
    else if (res.tipo == TIPO_ENUM) { res.enum_val = v->enum_val; res.enum_def = v->enum_def; }
    else res.n = v->val_num;
    return res;
}

static Result res_num(double n) {
    Result r = {0};
    r.tipo = TIPO_NUMERO;
    r.n = n;
    return r;
}

static void asignar_variable_desde_result(Result* args, int i, int v_idx, TipoExacto te) {
    vars[v_idx].tipo_ex = te;
    vars[v_idx].mut = MUT_VAR;
    vars[v_idx].tipo = args[i].tipo;
    vars[v_idx].obj = args[i].obj;
    if (vars[v_idx].tipo == TIPO_CADENA) vars[v_idx].val_str = args[i].s;
    else if (vars[v_idx].tipo == TIPO_ARRAY) vars[v_idx].val_array = args[i].a;
    else if (vars[v_idx].tipo == TIPO_MAP) vars[v_idx].val_map = args[i].m;
    else if (vars[v_idx].tipo == TIPO_INSTANCIA) vars[v_idx].val_inst = args[i].inst;
    else if (vars[v_idx].tipo == TIPO_ENUM) { vars[v_idx].enum_val = args[i].enum_val; vars[v_idx].enum_def = args[i].enum_def; }
    else vars[v_idx].val_num = args[i].n;
}

static void shell_debug(Chunk* chunk, uint8_t* ip) {
    int offset = (int)(ip - chunk->codigo);
    printf("\n--- DEBUGGER ITSUKI (offset: %d) ---\n", offset);
    char linea[128];
    while (1) {
        printf("debug> ");
        if (!fgets(linea, sizeof(linea), stdin)) break;
        if (linea[0] == 'p') {
            printf("Pila: ");
            for (struct Result* p = vm.pila; p < vm.pila_tope; p++) {
                if (p->tipo == TIPO_NUMERO) printf("[%g] ", p->n);
                else if (p->tipo == TIPO_CADENA) printf("[\"%s\"] ", p->s);
                else printf("[obj] ");
            }
            printf("\n");
        } else if (linea[0] == 's') {
            vm.step_mode = true;
            break;
        } else if (linea[0] == 'c') {
            vm.step_mode = false;
            break;
        } else if (linea[0] == 'h') {
            printf("Comandos: p (pila), s (paso a paso), c (continuar), h (ayuda)\n");
        }
    }
}

static void imprimir_result(Result r) {
    switch (r.tipo) {
        case TIPO_NUMERO: printf("%g", r.n); break;
        case TIPO_CADENA: printf("%s", r.s); break;
        case TIPO_BOOL: printf(r.n ? "verdadero" : "falso"); break;
        case TIPO_ARRAY: {
            printf("[");
            for (int i = 0; i < r.a->tamano; i++) {
                imprimir_result(r.a->elementos[i]);
                if (i < r.a->tamano - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case TIPO_MAP: {
            printf("{");
            int count = 0;
            for (int i = 0; i < 64; i++) {
                MapEntry* e = r.m->buckets[i];
                while (e) {
                    printf("%s: ", e->clave);
                    imprimir_result(*e->valor);
                    count++;
                    if (count < r.m->tamano) printf(", ");
                    e = e->next;
                }
            }
            printf("}");
            break;
        }
        case TIPO_NULO: printf("nulo"); break;
        case TIPO_FUNCION: printf("<funcion %d>", r.func_index); break;
        case TIPO_CLAUSURA: printf("<clausura>"); break;
        case TIPO_INSTANCIA: printf("<instancia de %s>", r.inst->clase->nombre); break;
        case TIPO_CLASE: printf("<clase>"); break;
        case TIPO_ENUM: {
            if (r.enum_val) {
                printf("%s.%s", r.enum_val->definicion->nombre, r.enum_val->definicion->variantes[r.enum_val->variante_index].nombre);
                if (r.enum_val->definicion->variantes[r.enum_val->variante_index].n_campos > 0) {
                    printf("(");
                    for (int i = 0; i < r.enum_val->definicion->variantes[r.enum_val->variante_index].n_campos; i++) {
                        imprimir_result(r.enum_val->valores[i]);
                        if (i < r.enum_val->definicion->variantes[r.enum_val->variante_index].n_campos - 1) printf(", ");
                    }
                    printf(")");
                }
            } else {
                printf("<enum %s>", r.enum_def->nombre);
            }
            break;
        }
        default: printf("<objeto>"); break;
    }
}

static char* nombre_base(const char* path) {
    const char* base = strrchr(path, '/');
    if (!base) base = strrchr(path, '\\');
    if (!base) base = path;
    else base++;
    char* res = my_strdup(base);
    char* dot = strrchr(res, '.');
    if (dot) *dot = 0;
    return res;
}

static ObjModulo* cargar_modulo(const char* path_orig);

Result vm_ejecutar(Chunk* c_rec) {
    if (!c_rec || !c_rec->codigo) return (Result){.tipo = TIPO_NULO};
    Chunk* chunk = c_rec;
    uint8_t* ip = chunk->codigo;
    Chunk* chunk_previo = vm.chunk;
    vm.chunk = chunk;
    
    uint8_t* ip_previo = vm.ip;

    if (setjmp(vm.recover_jmp)) {
        ip = vm.ip;
    }

#ifdef __GNUC__
    static void* dispatch_table[] = {
        &&do_OP_CONSTANTE, &&do_OP_NULO, &&do_OP_VERDADERO, &&do_OP_FALSO, &&do_OP_POP,
        &&do_OP_DEFINIR_GLOBAL, &&do_OP_OBTENER_GLOBAL, &&do_OP_ESTABLECER_GLOBAL,
        &&do_OP_OBTENER_LOCAL, &&do_OP_ESTABLECER_LOCAL,
        &&do_OP_SUMA, &&do_OP_RESTA, &&do_OP_MULT, &&do_OP_DIV,
        &&do_OP_NO, &&do_OP_IGUAL, &&do_OP_MAYOR, &&do_OP_MENOR,
        &&do_OP_SALTAR, &&do_OP_SALTAR_SI_FALSO, &&do_OP_LLAMAR, &&do_OP_COLA_LLAMAR,
        &&do_OP_MODULO, &&do_OP_PRINT, &&do_OP_RETORNAR,
        &&do_OP_CLASE, &&do_OP_NUEVA, &&do_OP_OBTENER_PROPIEDAD, &&do_OP_ESTABLECER_PROPIEDAD,
        &&do_OP_INVOQUE_METODO, &&do_OP_SUPER, &&do_OP_CLOSURE,
        &&do_OP_OBTENER_UPVALUE, &&do_OP_ESTABLECER_UPVALUE,
        &&do_OP_ITER_INIT, &&do_OP_ITER_NEXT, &&do_OP_ITER_DONE, &&do_OP_ARRAY_APPEND, &&do_OP_ARRAY_CREAR,
        &&do_OP_MAP_CREAR, &&do_OP_ESTABLECER_INDICE,
        &&do_OP_IMPORTAR, &&do_OP_LANZAR_ERROR, &&do_OP_DEFINIR_CON_TIPO,
        &&do_OP_ENUM, &&do_OP_ENUM_INSTANCIA, &&do_OP_ES_TIPO,
        &&do_OP_TRY_BEGIN, &&do_OP_TRY_END, &&do_OP_LANZAR, &&do_OP_FINALLY_END, &&do_OP_MARCAR_EXPORT,
        &&do_OP_MARCAR_MUT, &&do_OP_NOP, &&do_OP_CONST_SUMA, &&do_OP_CONST_RESTA, &&do_OP_CONST_MULT, &&do_OP_CONST_DIV
    };
    #define CASE(op) do_##op:
    #define DISPATCH_TABLE_LEN ((int)(sizeof(dispatch_table) / sizeof(dispatch_table[0])))
    #define DISPATCH() \
        do { \
            if (vm.debug_mode) { \
                int offset = (int)(ip - chunk->codigo); \
                for(int i=0; i<vm.n_breakpoints; i++) if(vm.breakpoints[i]==offset) shell_debug(chunk, ip); \
                if(vm.step_mode) shell_debug(chunk, ip); \
            } \
            vm.ip = ip; \
            do { \
                uint8_t op__ = *ip++; \
                if ((int)op__ < 0 || (int)op__ >= DISPATCH_TABLE_LEN) { \
                    lanzar_error(ERROR_SINTAXIS, "Opcode desconocido %d", (int)op__); \
                } \
                goto *dispatch_table[op__]; \
            } while (0); \
        } while (0)
    DISPATCH();
#else
    #define CASE(op) case op:
    #define DISPATCH() goto next_op
    while (1) {
    next_op:
        vm.ip = ip;
        if (vm.debug_mode) {
             int offset = (int)(ip - chunk->codigo);
             for(int i=0; i<vm.n_breakpoints; i++) if(vm.breakpoints[i]==offset) shell_debug(chunk, ip);
             if(vm.step_mode) shell_debug(chunk, ip);
        }
        uint8_t opcode = *ip++;
        switch (opcode) {
#endif

    CASE(OP_CONSTANTE) {
        uint8_t idx = *ip++;
        empujar(chunk->constantes[idx]);
        DISPATCH();
    }
    CASE(OP_NULO) { Result r = {0}; r.tipo = TIPO_NULO; empujar(r); DISPATCH(); }
    CASE(OP_VERDADERO) { empujar(res_num(1)); DISPATCH(); }
    CASE(OP_FALSO) { empujar(res_num(0)); DISPATCH(); }
    CASE(OP_POP) { extraer(); DISPATCH(); }
    CASE(OP_DEFINIR_GLOBAL) {
        uint8_t idx = *ip++; Result nombre = chunk->constantes[idx]; Result valor = extraer();
        int v_idx = hash_lookup(&ht_vars, nombre.s);
        if (v_idx == -1) {
            v_idx = n_v++; strcpy(vars[v_idx].nombre, nombre.s); hash_insert(&ht_vars, vars[v_idx].nombre, v_idx);
            vars[v_idx].tipo_ex = TEX_AUTO; vars[v_idx].es_publico = vm.export_proximo; vm.export_proximo = false;
        } else {
            if (vars[v_idx].mut != MUT_VAR || vm.mut_proximo != MUT_VAR) {
                lanzar_error(ERROR_TIPO, "Variable inmutable");
            }
            liberar_variable(&vars[v_idx]);
        }
        vars[v_idx].tipo = valor.tipo; vars[v_idx].obj = valor.obj;
        if (valor.tipo == TIPO_CADENA) vars[v_idx].val_str = valor.s;
        else if (valor.tipo == TIPO_ARRAY) vars[v_idx].val_array = valor.a;
        else if (valor.tipo == TIPO_MAP) vars[v_idx].val_map = valor.m;
        else if (valor.tipo == TIPO_INSTANCIA) vars[v_idx].val_inst = valor.inst;
        else if (valor.tipo == TIPO_ENUM) { vars[v_idx].enum_val = valor.enum_val; vars[v_idx].enum_def = valor.enum_def; }
        else vars[v_idx].val_num = valor.n;
        vars[v_idx].mut = vm.mut_proximo;
        vm.mut_proximo = MUT_VAR;
        DISPATCH();
    }
    CASE(OP_OBTENER_GLOBAL) {
        uint8_t idx = *ip++; Result nombre = chunk->constantes[idx];
        int op_off = (int)(ip - chunk->codigo) - 2;
        if (ENABLE_GLOBAL_CACHE && chunk->cache_global_idx && op_off >= 0 && op_off < chunk->cache_size) {
            int cached = chunk->cache_global_idx[op_off];
            uint32_t ver = chunk->cache_global_ver[op_off];
            if (cached >= 0 && cached < n_v && ver == vm.ht_vars_version && !strcmp(vars[cached].nombre, nombre.s)) {
                empujar(result_desde_variable(&vars[cached]));
                DISPATCH();
            }
        }
        int v_idx = hash_lookup(&ht_vars, nombre.s);
        if (v_idx == -1) {
            if (vm.clausura_actual) {
                for (int i=0; i<vm.clausura_actual->n_upvalues; i++) {
                    if (!strcmp(vm.clausura_actual->nombres_upvalues[i], nombre.s)) { empujar(vm.clausura_actual->upvalues[i]->closed); DISPATCH(); }
                }
            }
            int f_idx = hash_lookup(&ht_funcs, nombre.s);
            if (f_idx != -1) {
                if (strncmp(nombre.s, "socket", 6) == 0) fprintf(stderr, "[VM_GLO] Encontrada func socket: %s (idx=%d)\n", nombre.s, f_idx);
                empujar((Result){.tipo = TIPO_FUNCION, .func_index = f_idx}); DISPATCH();
            }
            int c_idx = hash_lookup(&ht_clases, nombre.s); if (c_idx != -1) { empujar((Result){.tipo = TIPO_CLASE, .clase_index = c_idx}); DISPATCH(); }
            lanzar_error(ERROR_NOMBRE, "Variable '%s' no definida", nombre.s);
        }
        if (ENABLE_GLOBAL_CACHE && chunk->cache_global_idx && op_off >= 0 && op_off < chunk->cache_size) {
            chunk->cache_global_idx[op_off] = v_idx;
            chunk->cache_global_ver[op_off] = vm.ht_vars_version;
        }
        empujar(result_desde_variable(&vars[v_idx]));
        DISPATCH();
    }
    CASE(OP_ESTABLECER_GLOBAL) {
        uint8_t idx = *ip++; Result valor = extraer();
        int v_idx = hash_lookup(&ht_vars, chunk->constantes[idx].s);
        if (v_idx == -1) lanzar_error(ERROR_NOMBRE, "Variable no definida");
        if (vars[v_idx].mut != MUT_VAR) lanzar_error(ERROR_TIPO, "Variable inmutable");
        if (vars[v_idx].tipo_ex != TEX_AUTO) aplicar_limites_tipo(&valor, vars[v_idx].tipo_ex);
        liberar_variable(&vars[v_idx]);
        vars[v_idx].tipo = valor.tipo; vars[v_idx].obj = valor.obj;
        if (valor.tipo == TIPO_CADENA) vars[v_idx].val_str = valor.s;
        else if (valor.tipo == TIPO_ARRAY) vars[v_idx].val_array = valor.a;
        else if (valor.tipo == TIPO_MAP) vars[v_idx].val_map = valor.m;
        else if (valor.tipo == TIPO_INSTANCIA) vars[v_idx].val_inst = valor.inst;
        else if (valor.tipo == TIPO_ENUM) { vars[v_idx].enum_val = valor.enum_val; vars[v_idx].enum_def = valor.enum_def; }
        else vars[v_idx].val_num = valor.n;
        empujar(valor);
        DISPATCH();
    }
    CASE(OP_OBTENER_LOCAL) { uint8_t offset = *ip++; empujar(vm.pila[offset]); DISPATCH(); }
    CASE(OP_ESTABLECER_LOCAL) { uint8_t offset = *ip++; vm.pila[offset] = extraer(); DISPATCH(); }
    CASE(OP_SUMA) {
        Result b = extraer(); Result a = extraer();
        if ((a.tipo == TIPO_NUMERO || a.tipo == TIPO_BOOL) &&
            (b.tipo == TIPO_NUMERO || b.tipo == TIPO_BOOL)) {
            Result res_n = {0};
            res_n.tipo = TIPO_NUMERO;
            res_n.n = a.n + b.n;
            empujar(res_n);
            DISPATCH();
        }
        if (a.tipo == TIPO_CADENA && b.tipo == TIPO_CADENA) {
            char* rs = malloc(strlen(a.s) + strlen(b.s) + 1);
            strcpy(rs, a.s); strcat(rs, b.s);
            Result res_s = gc_new_string(rs);
            empujar(res_s); free(rs);
            DISPATCH();
        }
        if (a.tipo == TIPO_CADENA || b.tipo == TIPO_CADENA) {
            Result s1 = result_to_string_gc(a);
            Result s2 = result_to_string_gc(b);
            char* rs = malloc(strlen(s1.s) + strlen(s2.s) + 1);
            strcpy(rs, s1.s); strcat(rs, s2.s);
            Result res_s = gc_new_string(rs);
            empujar(res_s); free(rs);
        } else {
            Result res_n = {0}; res_n.tipo = TIPO_NUMERO; res_n.n = a.n + b.n;
            empujar(res_n);
        }
        DISPATCH();
    }
    CASE(OP_CONST_SUMA) {
        uint8_t idx = *ip++; Result c = chunk->constantes[idx]; Result a = extraer();
        if ((a.tipo == TIPO_NUMERO || a.tipo == TIPO_BOOL) &&
            (c.tipo == TIPO_NUMERO || c.tipo == TIPO_BOOL)) {
            Result res_n = {0};
            res_n.tipo = TIPO_NUMERO;
            res_n.n = a.n + c.n;
            empujar(res_n);
            DISPATCH();
        }
        if (a.tipo == TIPO_CADENA && c.tipo == TIPO_CADENA) {
            char* rs = malloc(strlen(a.s) + strlen(c.s) + 1);
            strcpy(rs, a.s); strcat(rs, c.s);
            Result res_s = gc_new_string(rs);
            empujar(res_s); free(rs);
            DISPATCH();
        }
        if (a.tipo == TIPO_CADENA || c.tipo == TIPO_CADENA) {
            Result s1 = result_to_string_gc(a);
            Result s2 = result_to_string_gc(c);
            char* rs = malloc(strlen(s1.s) + strlen(s2.s) + 1);
            strcpy(rs, s1.s); strcat(rs, s2.s);
            Result res_s = gc_new_string(rs);
            empujar(res_s); free(rs);
        } else {
            empujar(res_num(a.n + c.n));
        }
        DISPATCH();
    }
    CASE(OP_CONST_RESTA) {
        uint8_t idx = *ip++; Result c = chunk->constantes[idx]; Result a = extraer();
        Result res_n = {0};
        res_n.tipo = TIPO_NUMERO;
        res_n.n = a.n - c.n;
        empujar(res_n);
        DISPATCH();
    }
    CASE(OP_CONST_MULT) {
        uint8_t idx = *ip++; Result c = chunk->constantes[idx]; Result a = extraer();
        Result res_n = {0};
        res_n.tipo = TIPO_NUMERO;
        res_n.n = a.n * c.n;
        empujar(res_n);
        DISPATCH();
    }
    CASE(OP_CONST_DIV) {
        uint8_t idx = *ip++; Result c = chunk->constantes[idx]; Result a = extraer();
        if (c.n == 0) lanzar_error(ERROR_DIVISION_CERO, "Div por 0");
        Result res_n = {0};
        res_n.tipo = TIPO_NUMERO;
        res_n.n = a.n / c.n;
        empujar(res_n);
        DISPATCH();
    }
    CASE(OP_RESTA) { Result b = extraer(); Result a = extraer(); empujar(res_num(a.n - b.n)); DISPATCH(); }
    CASE(OP_MULT) { Result b = extraer(); Result a = extraer(); empujar(res_num(a.n * b.n)); DISPATCH(); }
    CASE(OP_DIV) { Result b = extraer(); Result a = extraer(); if (b.n == 0) lanzar_error(ERROR_DIVISION_CERO, "Div por 0"); empujar(res_num(a.n / b.n)); DISPATCH(); }
    CASE(OP_MODULO) { Result b = extraer(); Result a = extraer(); empujar(res_num(fmod(a.n, b.n))); DISPATCH(); }
    CASE(OP_NO) { Result a = extraer(); empujar(res_num((a.n == 0) ? 1 : 0)); DISPATCH(); }
    CASE(OP_IGUAL) {
        Result b = extraer(); Result a = extraer(); bool m = false;
        if ((a.tipo == TIPO_NUMERO || a.tipo == TIPO_BOOL) &&
            (b.tipo == TIPO_NUMERO || b.tipo == TIPO_BOOL)) {
            m = a.n == b.n;
        } else if (a.tipo == TIPO_CADENA && b.tipo == TIPO_CADENA) {
            m = (a.s == b.s) ? true : !strcmp(a.s, b.s);
        }
        empujar(res_num(m ? 1 : 0)); DISPATCH();
    }
    CASE(OP_MAYOR) { Result b = extraer(); Result a = extraer(); empujar(res_num((a.n > b.n) ? 1 : 0)); DISPATCH(); }
    CASE(OP_MENOR) { Result b = extraer(); Result a = extraer(); empujar(res_num((a.n < b.n) ? 1 : 0)); DISPATCH(); }
    CASE(OP_SALTAR) { uint16_t off = (*ip++ << 8); off |= *ip++; ip = chunk->codigo + off; DISPATCH(); }
    CASE(OP_SALTAR_SI_FALSO) { uint16_t off = (*ip++ << 8); off |= *ip++; if (extraer().n == 0) ip = chunk->codigo + off; DISPATCH(); }

    CASE(OP_LLAMAR) {
        uint8_t na = *ip++; Result fv = extraer();
        Result args[16]; for (int i = na - 1; i >= 0; i--) args[i] = extraer();

        if (fv.tipo == TIPO_ENUM && fv.enum_def && fv.enum_val) {
            int vi = (int)(uintptr_t)fv.enum_val - 1; EnumDef* ed = fv.enum_def;
            if (ed->variantes[vi].n_campos != na) lanzar_error(ERROR_TIPO, "Campos incorrectos");
            EnumValor* ev = gc_alloc(sizeof(EnumValor), OBJ_ENUM_VAL); ev->definicion = ed; ev->variante_index = vi;
            ev->valores = malloc(sizeof(Result) * na); for(int i=0; i<na; i++) ev->valores[i] = args[i];
            empujar((Result){.tipo = TIPO_ENUM, .enum_val = ev, .obj = (Obj*)ev}); DISPATCH();
        }

        if (fv.tipo != TIPO_FUNCION && fv.tipo != TIPO_CLAUSURA) lanzar_error(ERROR_TIPO, "No es funcion");
        int fi = (fv.tipo == TIPO_FUNCION) ? fv.func_index : fv.clausura->func_index;
        for (int i=0; i<funcs[fi].n_params && i<na; i++) if (funcs[fi].param_tipos[i] != TEX_AUTO) aplicar_limites_tipo(&args[i], funcs[fi].param_tipos[i]);

        if (es_funcion_builtin(funcs[fi].nombre)) {
            empujar(ejecutar_builtin(funcs[fi].nombre, args, na));
        }
        else {
            if (vm.jit_enabled && fv.tipo == TIPO_FUNCION && funcs[fi].chunk_bytecode && funcs[fi].jit_state >= 0) {
                bool args_ok = (na >= funcs[fi].n_params);
                if (args_ok) {
                    for (int i = 0; i < funcs[fi].n_params; i++) {
                        if (args[i].tipo != TIPO_NUMERO && args[i].tipo != TIPO_BOOL) { args_ok = false; break; }
                    }
                }
                if (args_ok) {
                    funcs[fi].jit_calls++;
                    if (!funcs[fi].jit_ptr && funcs[fi].jit_calls >= vm.jit_hot_threshold) {
                        if (!jit_compilar_funcion(fi)) funcs[fi].jit_state = -1;
                    }
                    if (funcs[fi].jit_ptr && funcs[fi].jit_state == 1) {
                        Result res_jit;
                        if (jit_ejecutar_funcion(fi, args, na, &res_jit)) {
                            if (funcs[fi].tipo_retorno != TEX_AUTO) aplicar_limites_tipo(&res_jit, funcs[fi].tipo_retorno);
                            empujar(res_jit);
                            DISPATCH();
                        }
                    }
                }
            }
            HashNode* saved; hash_enter_scope(&saved); int old_nv = n_v;
            for (int i=0; i<funcs[fi].n_params && i<na; i++) {
                int v = n_v++; strcpy(vars[v].nombre, funcs[fi].params[i]); vars[v].tipo_ex = funcs[fi].param_tipos[i]; vars[v].mut = MUT_VAR;
                vars[v].tipo = args[i].tipo; vars[v].obj = args[i].obj;
                if (vars[v].tipo == TIPO_CADENA) vars[v].val_str = args[i].s; else if (vars[v].tipo == TIPO_ARRAY) vars[v].val_array = args[i].a;
                else if (vars[v].tipo == TIPO_MAP) vars[v].val_map = args[i].m; else if (vars[v].tipo == TIPO_INSTANCIA) vars[v].val_inst = args[i].inst;
                else if (vars[v].tipo == TIPO_ENUM) { vars[v].enum_val = args[i].enum_val; vars[v].enum_def = args[i].enum_def; } else vars[v].val_num = args[i].n;
                hash_insert(&ht_vars, vars[v].nombre, v);
            }
            Closure* prev_cl = vm.clausura_actual; vm.clausura_actual = (fv.tipo == TIPO_CLAUSURA) ? fv.clausura : NULL;
            Result res;
            if (vm.profiling_mode) {
                clock_t s = clock(); res = vm_ejecutar(funcs[fi].chunk_bytecode); clock_t e = clock();
                double el = (double)(e - s) / CLOCKS_PER_SEC;
                int pi = -1; for(int k=0; k<n_perfil; k++) if(!strcmp(perfil_datos[k].nombre_func, funcs[fi].nombre)) { pi = k; break; }
                if(pi == -1 && n_perfil < MAX_FUNCS) { pi = n_perfil++; strcpy(perfil_datos[pi].nombre_func, funcs[fi].nombre); perfil_datos[pi].tiempo_total = 0; perfil_datos[pi].llamadas = 0; }
                if(pi != -1) { perfil_datos[pi].tiempo_total += el; perfil_datos[pi].llamadas++; }
            } else res = vm_ejecutar(funcs[fi].chunk_bytecode);
            if (funcs[fi].tipo_retorno != TEX_AUTO) aplicar_limites_tipo(&res, funcs[fi].tipo_retorno);
            vm.clausura_actual = prev_cl; for (int i=old_nv; i<n_v; i++) liberar_variable(&vars[i]); n_v = old_nv; hash_exit_scope(&saved); empujar(res);
        }
        DISPATCH();
    }
    CASE(OP_COLA_LLAMAR) {
        uint8_t na = *ip++; Result fv = extraer();
        Result args[16]; for (int i = na - 1; i >= 0; i--) args[i] = extraer();
        if (fv.tipo != TIPO_FUNCION) lanzar_error(ERROR_TIPO, "TCO solo funciones simples");
        int fi = fv.func_index;
        if (es_funcion_builtin(funcs[fi].nombre)) {
            Result res_b = ejecutar_builtin(funcs[fi].nombre, args, na);
            vm.ip = ip_previo;
            vm.chunk = chunk_previo;
            return res_b;
        }
        else {
            for (int i=0; i<na && i<funcs[fi].n_params; i++) {
                int vi = hash_lookup(&ht_vars, funcs[fi].params[i]);
                if (vi != -1) {
                    liberar_variable(&vars[vi]); vars[vi].tipo = args[i].tipo; vars[vi].obj = args[i].obj;
                    if (args[i].tipo == TIPO_CADENA) vars[vi].val_str = args[i].s; else vars[vi].val_num = args[i].n;
                }
            }
            chunk = funcs[fi].chunk_bytecode; ip = chunk->codigo;
        }
        DISPATCH();
    }
    CASE(OP_RETORNAR) {
        Result res_ret = (vm.pila_tope > vm.pila) ? extraer() : (Result){.tipo = TIPO_NULO};
        vm.ip = ip_previo;
        vm.chunk = chunk_previo;
        return res_ret;
    }
    CASE(OP_PRINT) {
        uint8_t na = *ip++; Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer();
        for (int i=0; i<na; i++) {
            imprimir_result(args[i]);
        }
        printf("\n"); fflush(stdout); DISPATCH();
    }
    CASE(OP_CLASE) {
        uint8_t ni = *ip++; uint8_t pi = *ip++; uint8_t nm = *ip++; int ci = n_clases++;
        strcpy(clases[ci].nombre, chunk->constantes[ni].s);
        int pix = (pi > 0) ? hash_lookup(&ht_clases, chunk->constantes[pi-1].s) : -1;
        clases[ci].clase_padre = (pix != -1) ? &clases[pix] : NULL;
        int npp = clases[ci].clase_padre ? clases[ci].clase_padre->n_propiedades : 0;
        clases[ci].n_propiedades = npp; clases[ci].n_metodos = 0; clases[ci].n_estaticos = 0;
        clases[ci].propiedades = malloc(sizeof(char*) * (npp + nm)); clases[ci].props_privadas = malloc(sizeof(bool) * (npp + nm));
        if (clases[ci].clase_padre) for (int i=0; i<npp; i++) { clases[ci].propiedades[i] = my_strdup(clases[ci].clase_padre->propiedades[i]); clases[ci].props_privadas[i] = clases[ci].clase_padre->props_privadas[i]; }
        clases[ci].indices_metodos = malloc(sizeof(int) * nm); clases[ci].metodos_privados = malloc(sizeof(bool) * nm);
        clases[ci].metodos_estaticos = malloc(sizeof(bool) * nm); clases[ci].nombres_estaticos = malloc(sizeof(char*) * nm); clases[ci].valores_estaticos = malloc(sizeof(Result) * nm);
        for (int i=0; i<nm; i++) {
            uint8_t mni = *ip++; uint8_t f = *ip++; char* mn = chunk->constantes[mni].s;
            bool is_s = f & 0x01, is_p = f & 0x02, is_f = f & 0x04;
            if (is_s) { clases[ci].nombres_estaticos[clases[ci].n_estaticos] = my_strdup(mn); clases[ci].valores_estaticos[clases[ci].n_estaticos] = is_f ? (Result){.tipo=TIPO_FUNCION, .func_index=hash_lookup(&ht_funcs, mn)} : extraer(); clases[ci].n_estaticos++; }
            else if (is_f) { clases[ci].indices_metodos[clases[ci].n_metodos] = hash_lookup(&ht_funcs, mn); clases[ci].metodos_privados[clases[ci].n_metodos++] = is_p; }
            else { clases[ci].propiedades[clases[ci].n_propiedades] = my_strdup(mn); clases[ci].props_privadas[clases[ci].n_propiedades++] = is_p; extraer(); }
        }
        hash_insert(&ht_clases, clases[ci].nombre, ci); clases[ci].es_publico = vm.export_proximo; vm.export_proximo = false; DISPATCH();
    }
    CASE(OP_NUEVA) {
        uint8_t idx = *ip++; uint8_t na = *ip++; int ci = hash_lookup(&ht_clases, chunk->constantes[idx].s);
        if (ci == -1) { int vi = hash_lookup(&ht_vars, chunk->constantes[idx].s); if (vi != -1 && vars[vi].tipo == TIPO_CLASE) ci = vars[vi].clase_index; }
        if (ci == -1) lanzar_error(ERROR_NOMBRE, "Clase no def");
        Instancia* inst = gc_alloc(sizeof(Instancia), OBJ_INSTANCIA); inst->clase = &clases[ci];
        inst->valores_propiedades = malloc(sizeof(Result) * inst->clase->n_propiedades);
        for(int i=0; i<inst->clase->n_propiedades; i++) inst->valores_propiedades[i] = (Result){.tipo = TIPO_NULO};
        int ii = -1; Clase* cur = inst->clase;
        while(cur && ii == -1) { for(int i=0; i<cur->n_metodos; i++) if(!strcmp(funcs[cur->indices_metodos[i]].nombre, "init")) { ii = cur->indices_metodos[i]; break; } cur = cur->clase_padre; }
        if (ii != -1) {
            Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer();
            HashNode* s; hash_enter_scope(&s); int onv = n_v; int ve = n_v++; strcpy(vars[ve].nombre, "este"); vars[ve].tipo = TIPO_INSTANCIA; vars[ve].val_inst = inst; vars[ve].obj = (Obj*)inst; vars[ve].mut = MUT_VAR; hash_insert(&ht_vars, "este", ve);
            for (int i=0; i<funcs[ii].n_params && i<na; i++) { int v = n_v++; strcpy(vars[v].nombre, funcs[ii].params[i]); vars[v].tipo = args[i].tipo; vars[v].obj = args[i].obj; vars[v].mut = MUT_VAR; if(vars[v].tipo==TIPO_CADENA) vars[v].val_str=args[i].s; else vars[v].val_num=args[i].n; hash_insert(&ht_vars, vars[v].nombre, v); }
            vm_ejecutar(funcs[ii].chunk_bytecode); for(int i=onv; i<n_v; i++) liberar_variable(&vars[i]); n_v = onv; hash_exit_scope(&s);
        }
        empujar((Result){.tipo = TIPO_INSTANCIA, .inst = inst, .obj = (Obj*)inst}); DISPATCH();
    }
    CASE(OP_OBTENER_PROPIEDAD) {
        int op_off = (int)(ip - chunk->codigo) - 1;
        Result pn = extraer(); Result o = extraer();
        if (o.tipo == TIPO_INSTANCIA) {
            Instancia* inst = o.inst;
            if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_prop_cls[op_off] == inst->clase && chunk->cache_prop_name[op_off] == pn.s) {
                    int idx = chunk->cache_prop_index[op_off];
                    uint8_t kind = chunk->cache_prop_kind[op_off];
                    if (kind == PROP_CACHE_PROP && idx >= 0 && idx < inst->clase->n_propiedades) {
                        empujar(inst->valores_propiedades[idx]); DISPATCH();
                    }
                    if (kind == PROP_CACHE_METHOD && idx >= 0 && idx < n_f) {
                        empujar((Result){.tipo=TIPO_FUNCION, .func_index=idx}); DISPATCH();
                    }
                }
            }
            for (int i=0; i<inst->clase->n_propiedades; i++) if(!strcmp(inst->clase->propiedades[i], pn.s)) {
                if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                    chunk->cache_prop_cls[op_off] = inst->clase;
                    chunk->cache_prop_name[op_off] = pn.s;
                    chunk->cache_prop_index[op_off] = i;
                    chunk->cache_prop_kind[op_off] = PROP_CACHE_PROP;
                }
                empujar(inst->valores_propiedades[i]); DISPATCH();
            }
            Clase* cur = inst->clase;
            while(cur) {
                for(int i=0; i<cur->n_metodos; i++) if(!strcmp(funcs[cur->indices_metodos[i]].nombre, pn.s)) {
                    if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                        chunk->cache_prop_cls[op_off] = inst->clase;
                        chunk->cache_prop_name[op_off] = pn.s;
                        chunk->cache_prop_index[op_off] = cur->indices_metodos[i];
                        chunk->cache_prop_kind[op_off] = PROP_CACHE_METHOD;
                    }
                    empujar((Result){.tipo=TIPO_FUNCION, .func_index=cur->indices_metodos[i]}); DISPATCH();
                }
                cur = cur->clase_padre;
            }
            lanzar_error(ERROR_NOMBRE, "Miembro no enc");
        } else if (o.tipo == TIPO_CLASE) {
            Clase* cls = &clases[o.clase_index];
            if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_prop_cls[op_off] == cls && chunk->cache_prop_name[op_off] == pn.s &&
                    chunk->cache_prop_kind[op_off] == PROP_CACHE_STATIC) {
                    int idx = chunk->cache_prop_index[op_off];
                    if (idx >= 0 && idx < cls->n_estaticos) {
                        empujar(cls->valores_estaticos[idx]); DISPATCH();
                    }
                }
            }
            for (int i=0; i<cls->n_estaticos; i++) if(!strcmp(cls->nombres_estaticos[i], pn.s)) {
                if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                    chunk->cache_prop_cls[op_off] = cls;
                    chunk->cache_prop_name[op_off] = pn.s;
                    chunk->cache_prop_index[op_off] = i;
                    chunk->cache_prop_kind[op_off] = PROP_CACHE_STATIC;
                }
                empujar(cls->valores_estaticos[i]); DISPATCH();
            }
            lanzar_error(ERROR_NOMBRE, "Miembro no enc");
        } else if (o.tipo == TIPO_ENUM && o.enum_def) {
            EnumDef* ed = o.enum_def; for(int i=0; i<ed->n_variantes; i++) if(!strcmp(ed->variantes[i].nombre, pn.s)) { if(ed->variantes[i].n_campos==0) { EnumValor* ev = gc_alloc(sizeof(EnumValor), OBJ_ENUM_VAL); ev->definicion=ed; ev->variante_index=i; ev->valores=NULL; empujar((Result){.tipo=TIPO_ENUM, .enum_val=ev, .obj=(Obj*)ev}); } else empujar((Result){.tipo=TIPO_ENUM, .enum_def=ed, .enum_val=(EnumValor*)(uintptr_t)(i+1)}); DISPATCH(); }
            lanzar_error(ERROR_NOMBRE, "Variante no enc");
        } else if (o.tipo == TIPO_ARRAY) {
            if (pn.tipo != TIPO_NUMERO && pn.tipo != TIPO_BOOL) {
                lanzar_error(ERROR_TIPO, "Indice debe ser numero");
            }
            int idx = (int)pn.n;
            if (idx < 0 || idx >= o.a->tamano) {
                lanzar_error(ERROR_INDICE, "Índice fuera de rango");
            }
            empujar(o.a->elementos[idx]); DISPATCH();
        } else if (o.tipo == TIPO_MAP) {
            if (pn.tipo != TIPO_CADENA) {
                lanzar_error(ERROR_TIPO, "Clave debe ser cadena");
            }
            Result v = map_obtener(o.m, pn.s);
            empujar(v); DISPATCH();
        } else if (o.tipo == TIPO_MODULO) { ObjModulo* m = (ObjModulo*)o.obj; Result v = map_obtener(m->exports, pn.s); if(v.tipo==TIPO_NULO) lanzar_error(ERROR_NOMBRE, "No en mod"); empujar(v); DISPATCH(); }
        DISPATCH();
    }
    CASE(OP_ESTABLECER_PROPIEDAD) {
        int op_off = (int)(ip - chunk->codigo) - 1;
        uint8_t pi = *ip++; Result pn = chunk->constantes[pi]; Result o = extraer(); Result v = extraer();
        if (o.tipo == TIPO_INSTANCIA) {
            Instancia* inst = o.inst;
            if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_prop_cls[op_off] == inst->clase && chunk->cache_prop_name[op_off] == pn.s &&
                    chunk->cache_prop_kind[op_off] == PROP_CACHE_PROP) {
                    int idx = chunk->cache_prop_index[op_off];
                    if (idx >= 0 && idx < inst->clase->n_propiedades) {
                        inst->valores_propiedades[idx] = v; empujar(v); DISPATCH();
                    }
                }
            }
            for (int i=0; i<inst->clase->n_propiedades; i++) if(!strcmp(inst->clase->propiedades[i], pn.s)) {
                if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                    chunk->cache_prop_cls[op_off] = inst->clase;
                    chunk->cache_prop_name[op_off] = pn.s;
                    chunk->cache_prop_index[op_off] = i;
                    chunk->cache_prop_kind[op_off] = PROP_CACHE_PROP;
                }
                inst->valores_propiedades[i] = v; empujar(v); DISPATCH();
            }
        }
        else if (o.tipo == TIPO_CLASE) {
            Clase* cls = &clases[o.clase_index];
            if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_prop_cls[op_off] == cls && chunk->cache_prop_name[op_off] == pn.s &&
                    chunk->cache_prop_kind[op_off] == PROP_CACHE_STATIC) {
                    int idx = chunk->cache_prop_index[op_off];
                    if (idx >= 0 && idx < cls->n_estaticos) {
                        cls->valores_estaticos[idx] = v; empujar(v); DISPATCH();
                    }
                }
            }
            for(int i=0; i<cls->n_estaticos; i++) if(!strcmp(cls->nombres_estaticos[i], pn.s)) {
                if (pn.tipo == TIPO_CADENA && chunk->cache_prop_cls && op_off >= 0 && op_off < chunk->cache_size) {
                    chunk->cache_prop_cls[op_off] = cls;
                    chunk->cache_prop_name[op_off] = pn.s;
                    chunk->cache_prop_index[op_off] = i;
                    chunk->cache_prop_kind[op_off] = PROP_CACHE_STATIC;
                }
                cls->valores_estaticos[i]=v; empujar(v); DISPATCH();
            }
        }
        else if (o.tipo == TIPO_ARRAY) {
            if (pn.tipo != TIPO_NUMERO && pn.tipo != TIPO_BOOL) lanzar_error(ERROR_TIPO, "Indice debe ser numero");
            int idx = (int)pn.n;
            if (idx < 0 || idx >= o.a->tamano) lanzar_error(ERROR_INDICE, "Índice fuera de rango");
            o.a->elementos[idx] = v; empujar(v); DISPATCH();
        }
        else if (o.tipo == TIPO_MAP) {
            if (pn.tipo != TIPO_CADENA) lanzar_error(ERROR_TIPO, "Clave debe ser cadena");
            map_establecer(o.m, pn.s, v); empujar(v); DISPATCH();
        }
        lanzar_error(ERROR_NOMBRE, "Prop no enc"); DISPATCH();
    }
    CASE(OP_INVOQUE_METODO) {
        int op_off = (int)(ip - chunk->codigo) - 1;
        uint8_t mi = *ip++; uint8_t na = *ip++; Result pn = chunk->constantes[mi]; Result o = extraer();
        if (o.tipo == TIPO_INSTANCIA) {
            Instancia* inst = o.inst;
            int fi = -1;
            if (chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_invoke_cls[op_off] == inst->clase &&
                    chunk->cache_invoke_kind[op_off] == INVOKE_CACHE_INSTANCE) {
                    int cached = chunk->cache_invoke_func[op_off];
                    if (cached >= 0 && cached < n_f) fi = cached;
                }
            }
            if (fi == -1) {
                Clase* cur = inst->clase;
                while(cur && fi == -1) {
                    for(int i=0; i<cur->n_metodos; i++) if(!strcmp(funcs[cur->indices_metodos[i]].nombre, pn.s)) {
                        fi = cur->indices_metodos[i];
                        break;
                    }
                    cur = cur->clase_padre;
                }
                if (fi != -1 && chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
                    chunk->cache_invoke_cls[op_off] = inst->clase;
                    chunk->cache_invoke_func[op_off] = fi;
                    chunk->cache_invoke_kind[op_off] = INVOKE_CACHE_INSTANCE;
                }
            }
            if (fi != -1) {
                Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer(); HashNode* s; hash_enter_scope(&s); int onv = n_v; int ve = n_v++; strcpy(vars[ve].nombre, "este"); vars[ve].tipo = TIPO_INSTANCIA; vars[ve].val_inst = inst; vars[ve].obj = (Obj*)inst; vars[ve].mut = MUT_VAR; hash_insert(&ht_vars, "este", ve);
                for(int i=0; i<funcs[fi].n_params && i<na; i++) { int v = n_v++; strcpy(vars[v].nombre, funcs[fi].params[i]); vars[v].tipo = args[i].tipo; vars[v].obj = args[i].obj; vars[v].mut = MUT_VAR; if(vars[v].tipo==TIPO_CADENA) vars[v].val_str=args[i].s; else vars[v].val_num=args[i].n; hash_insert(&ht_vars, vars[v].nombre, v); }
                Result res = vm_ejecutar(funcs[fi].chunk_bytecode); for(int i=onv; i<n_v; i++) liberar_variable(&vars[i]); n_v = onv; hash_exit_scope(&s); empujar(res);
            } else lanzar_error(ERROR_NOMBRE, "Metodo no enc");
        } else if (o.tipo == TIPO_CLASE) {
            Clase* cls = &clases[o.clase_index];
            Result func_res = {0};
            int fi = -1;
            if (chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
                if (chunk->cache_invoke_cls[op_off] == cls &&
                    chunk->cache_invoke_kind[op_off] == INVOKE_CACHE_CLASS) {
                    int cached = chunk->cache_invoke_func[op_off];
                    if (cached >= 0 && cached < n_f) {
                        fi = cached;
                        func_res.tipo = TIPO_FUNCION;
                        func_res.func_index = cached;
                    }
                }
            }
            if (fi == -1) {
                for (int i=0; i<cls->n_estaticos; i++) {
                    if (!strcmp(cls->nombres_estaticos[i], pn.s)) {
                        func_res = cls->valores_estaticos[i];
                        if (func_res.tipo == TIPO_FUNCION) fi = func_res.func_index;
                        else if (func_res.tipo == TIPO_CLAUSURA) fi = func_res.clausura->func_index;
                        else lanzar_error(ERROR_TIPO, "No es funcion");
                        if (func_res.tipo == TIPO_FUNCION && chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
                            chunk->cache_invoke_cls[op_off] = cls;
                            chunk->cache_invoke_func[op_off] = fi;
                            chunk->cache_invoke_kind[op_off] = INVOKE_CACHE_CLASS;
                        }
                        break;
                    }
                }
            }
            if (fi == -1) lanzar_error(ERROR_NOMBRE, "Metodo no enc");

            Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer();
            for (int i=0; i<funcs[fi].n_params && i<na; i++) {
                if (funcs[fi].param_tipos[i] != TEX_AUTO) aplicar_limites_tipo(&args[i], funcs[fi].param_tipos[i]);
            }
            if (es_funcion_builtin(funcs[fi].nombre)) {
                empujar(ejecutar_builtin(funcs[fi].nombre, args, na));
            } else {
                HashNode* s; hash_enter_scope(&s); int onv = n_v;
                for(int i=0; i<funcs[fi].n_params && i<na; i++) {
                    int v = n_v++; strcpy(vars[v].nombre, funcs[fi].params[i]);
                    asignar_variable_desde_result(args, i, v, funcs[fi].param_tipos[i]);
                    hash_insert(&ht_vars, vars[v].nombre, v);
                }
                Closure* prev_cl = vm.clausura_actual;
                vm.clausura_actual = (func_res.tipo == TIPO_CLAUSURA) ? func_res.clausura : NULL;
                Result res;
                if (vm.profiling_mode) {
                    clock_t s_t = clock(); res = vm_ejecutar(funcs[fi].chunk_bytecode); clock_t e_t = clock();
                    double el = (double)(e_t - s_t) / CLOCKS_PER_SEC;
                    int pi = -1; for(int k=0; k<n_perfil; k++) if(!strcmp(perfil_datos[k].nombre_func, funcs[fi].nombre)) { pi = k; break; }
                    if(pi == -1 && n_perfil < MAX_FUNCS) { pi = n_perfil++; strcpy(perfil_datos[pi].nombre_func, funcs[fi].nombre); perfil_datos[pi].tiempo_total = 0; perfil_datos[pi].llamadas = 0; }
                    if(pi != -1) { perfil_datos[pi].tiempo_total += el; perfil_datos[pi].llamadas++; }
                } else res = vm_ejecutar(funcs[fi].chunk_bytecode);
                if (funcs[fi].tipo_retorno != TEX_AUTO) aplicar_limites_tipo(&res, funcs[fi].tipo_retorno);
                vm.clausura_actual = prev_cl;
                for(int i=onv; i<n_v; i++) liberar_variable(&vars[i]);
                n_v = onv; hash_exit_scope(&s);
                empujar(res);
            }
        } else if (o.tipo == TIPO_MODULO) {
            ObjModulo* mod = (ObjModulo*)o.obj;
            Result func_res = map_obtener(mod->exports, pn.s);
            if (func_res.tipo == TIPO_NULO) lanzar_error(ERROR_NOMBRE, "Funcion no encontrada en modulo");
            if (func_res.tipo == TIPO_FUNCION || func_res.tipo == TIPO_CLAUSURA) {
                int fi = (func_res.tipo == TIPO_FUNCION) ? func_res.func_index : func_res.clausura->func_index;
                Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer();

                for (int i=0; i<funcs[fi].n_params && i<na; i++) {
                    if (funcs[fi].param_tipos[i] != TEX_AUTO) aplicar_limites_tipo(&args[i], funcs[fi].param_tipos[i]);
                }

                if (es_funcion_builtin(funcs[fi].nombre)) {
                    empujar(ejecutar_builtin(funcs[fi].nombre, args, na));
                } else {
                    HashNode* s; hash_enter_scope(&s); int onv = n_v;
                    for(int i=0; i<funcs[fi].n_params && i<na; i++) {
                        int v = n_v++; strcpy(vars[v].nombre, funcs[fi].params[i]);
                        asignar_variable_desde_result(args, i, v, funcs[fi].param_tipos[i]);
                        hash_insert(&ht_vars, vars[v].nombre, v);
                    }
                    Closure* prev_cl = vm.clausura_actual;
                    vm.clausura_actual = (func_res.tipo == TIPO_CLAUSURA) ? func_res.clausura : NULL;
                    Result res;
                    if (vm.profiling_mode) {
                        clock_t s_t = clock(); res = vm_ejecutar(funcs[fi].chunk_bytecode); clock_t e_t = clock();
                        double el = (double)(e_t - s_t) / CLOCKS_PER_SEC;
                        int pi = -1; for(int k=0; k<n_perfil; k++) if(!strcmp(perfil_datos[k].nombre_func, funcs[fi].nombre)) { pi = k; break; }
                        if(pi == -1 && n_perfil < MAX_FUNCS) { pi = n_perfil++; strcpy(perfil_datos[pi].nombre_func, funcs[fi].nombre); perfil_datos[pi].tiempo_total = 0; perfil_datos[pi].llamadas = 0; }
                        if(pi != -1) { perfil_datos[pi].tiempo_total += el; perfil_datos[pi].llamadas++; }
                    } else res = vm_ejecutar(funcs[fi].chunk_bytecode);
                    if (funcs[fi].tipo_retorno != TEX_AUTO) aplicar_limites_tipo(&res, funcs[fi].tipo_retorno);
                    vm.clausura_actual = prev_cl;
                    for(int i=onv; i<n_v; i++) liberar_variable(&vars[i]);
                    n_v = onv; hash_exit_scope(&s);
                    empujar(res);
                }
            } else {
                empujar(func_res);
            }
        }
        DISPATCH();
    }
    CASE(OP_SUPER) {
        int op_off = (int)(ip - chunk->codigo) - 1;
        uint8_t mi = *ip++; uint8_t na = *ip++; Result pn = chunk->constantes[mi]; int ve = hash_lookup(&ht_vars, "este"); if(ve==-1) lanzar_error(ERROR_NOMBRE, "super fuera de m");
        Instancia* inst = vars[ve].val_inst; Clase* pad = inst->clase->clase_padre; if(!pad) lanzar_error(ERROR_NOMBRE, "Sin padre");
        int fi = -1;
        if (chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
            if (chunk->cache_invoke_cls[op_off] == pad &&
                chunk->cache_invoke_kind[op_off] == INVOKE_CACHE_SUPER) {
                int cached = chunk->cache_invoke_func[op_off];
                if (cached >= 0 && cached < n_f) fi = cached;
            }
        }
        if (fi == -1) {
            Clase* cur = pad;
            while(cur && fi == -1) {
                for(int i=0; i<cur->n_metodos; i++) if(!strcmp(funcs[cur->indices_metodos[i]].nombre, pn.s)) {
                    fi = cur->indices_metodos[i]; break;
                }
                cur = cur->clase_padre;
            }
            if (fi != -1 && chunk->cache_invoke_cls && op_off >= 0 && op_off < chunk->cache_size) {
                chunk->cache_invoke_cls[op_off] = pad;
                chunk->cache_invoke_func[op_off] = fi;
                chunk->cache_invoke_kind[op_off] = INVOKE_CACHE_SUPER;
            }
        }
        if (fi != -1) {
            Result args[16]; for (int i=na-1; i>=0; i--) args[i] = extraer(); HashNode* s; hash_enter_scope(&s); int onv = n_v; int ven = n_v++; strcpy(vars[ven].nombre, "este"); vars[ven].tipo = TIPO_INSTANCIA; vars[ven].val_inst = inst; vars[ven].obj = (Obj*)inst; vars[ven].mut = MUT_VAR; hash_insert(&ht_vars, "este", ven);
            for(int i=0; i<funcs[fi].n_params && i<na; i++) { int v = n_v++; strcpy(vars[v].nombre, funcs[fi].params[i]); vars[v].tipo = args[i].tipo; vars[v].obj = args[i].obj; vars[v].mut = MUT_VAR; if(vars[v].tipo==TIPO_CADENA) vars[v].val_str=args[i].s; else vars[v].val_num=args[i].n; hash_insert(&ht_vars, vars[v].nombre, v); }
            Result res = vm_ejecutar(funcs[fi].chunk_bytecode); for(int i=onv; i<n_v; i++) liberar_variable(&vars[i]); n_v = onv; hash_exit_scope(&s); empujar(res);
        } DISPATCH();
    }
    CASE(OP_CLOSURE) {
        uint8_t fi = *ip++; uint8_t nup = *ip++; (void)nup; Closure* cl = gc_alloc(sizeof(Closure), OBJ_CLAUSURA); cl->func_index = fi;
        int nt = 0; for(int i=0; i<512; i++) { HashNode* n = ht_vars.buckets[i]; while(n) { nt++; n = n->next; } } if(vm.clausura_actual) nt += vm.clausura_actual->n_upvalues;
        cl->n_upvalues = nt; cl->upvalues = nt?malloc(sizeof(Upvalue*)*nt):NULL; cl->nombres_upvalues = nt?malloc(sizeof(char*)*nt):NULL;
        int ix = 0; for(int i=0; i<512; i++) { HashNode* n = ht_vars.buckets[i]; while(n) { cl->nombres_upvalues[ix]=my_strdup(n->nombre); Upvalue* uv = gc_alloc(sizeof(Upvalue), OBJ_UPVALUE); uv->location=&uv->closed; uv->closed=result_desde_variable(&vars[n->index]); cl->upvalues[ix]=uv; ix++; n=n->next; } }
        if(vm.clausura_actual) for(int i=0; i<vm.clausura_actual->n_upvalues; i++) { cl->nombres_upvalues[ix]=my_strdup(vm.clausura_actual->nombres_upvalues[i]); cl->upvalues[ix]=vm.clausura_actual->upvalues[i]; ix++; }
        empujar((Result){.tipo=TIPO_CLAUSURA, .clausura=cl, .obj=(Obj*)cl}); DISPATCH();
    }
    CASE(OP_OBTENER_UPVALUE) { uint8_t ix = *ip++; if(vm.clausura_actual) empujar(vm.clausura_actual->upvalues[ix]->closed); DISPATCH(); }
    CASE(OP_ESTABLECER_UPVALUE) { uint8_t ix = *ip++; if(vm.clausura_actual) vm.clausura_actual->upvalues[ix]->closed = extraer(); DISPATCH(); }
    CASE(OP_ITER_INIT) { Result iter = extraer(); if(iter.tipo!=TIPO_ARRAY) lanzar_error(ERROR_TIPO, "Array req"); vm.iterator_state.iterable=iter; vm.iterator_state.index=0; vm.iterator_state.size=iter.a->tamano; DISPATCH(); }
    CASE(OP_ITER_NEXT) { if(vm.iterator_state.index < vm.iterator_state.size) { Result v = array_obtener(vm.iterator_state.iterable.a, vm.iterator_state.index++); empujar(v); empujar(res_num(1)); } else empujar(res_num(0)); DISPATCH(); }
    CASE(OP_ITER_DONE) { empujar(res_num((vm.iterator_state.index>=vm.iterator_state.size)?1:0)); DISPATCH(); }
    CASE(OP_ARRAY_APPEND) { Result v = extraer(); Result a = extraer(); if(a.tipo!=TIPO_ARRAY) lanzar_error(ERROR_TIPO, "Array req"); array_agregar(a.a, v); empujar(a); DISPATCH(); }
    CASE(OP_ARRAY_CREAR) { empujar(gc_new_array(10)); DISPATCH(); }
    CASE(OP_MAP_CREAR) {
        Map* m = map_crear();
        Result r = {0};
        r.tipo = TIPO_MAP;
        r.m = m;
        r.obj = (Obj*)m;
        empujar(r);
        DISPATCH();
    }
    CASE(OP_ESTABLECER_INDICE) {
        Result valor = extraer();
        Result indice = extraer();
        Result receptor = extraer();
        if (receptor.tipo == TIPO_ARRAY) {
            if (indice.tipo != TIPO_NUMERO) lanzar_error(ERROR_TIPO, "Indice de array debe ser numero");
            int ix = (int)indice.n;
            if (ix < 0 || ix >= receptor.a->tamano) lanzar_error(ERROR_INDICE, "Indice fuera de rango");
            receptor.a->elementos[ix] = valor;
            empujar(valor);
            DISPATCH();
        }
        if (receptor.tipo == TIPO_MAP) {
            if (indice.tipo != TIPO_CADENA) lanzar_error(ERROR_TIPO, "Clave de mapa debe ser cadena");
            map_establecer(receptor.m, indice.s, valor);
            empujar(valor);
            DISPATCH();
        }
        lanzar_error(ERROR_TIPO, "Receptor no indexable");
        DISPATCH();
    }
    CASE(OP_IMPORTAR) {
        uint8_t pi = *ip++; uint8_t ai = *ip++; uint8_t nn = *ip++; char* path = chunk->constantes[pi].s; ObjModulo* m = cargar_modulo(path);
        if(nn==0) { char* alias = (ai==0xff)?NULL:chunk->constantes[ai].s; char* fa = alias?my_strdup(alias):nombre_base(path); int vi=hash_lookup(&ht_vars, fa); if(vi==-1) vi=n_v++; strcpy(vars[vi].nombre, fa); vars[vi].tipo=TIPO_MODULO; vars[vi].obj=(Obj*)m; vars[vi].mut = MUT_VAR; hash_insert(&ht_vars, vars[vi].nombre, vi); free(fa); }
        else for(int i=0; i<nn; i++) { uint8_t nci = *ip++; char* n = chunk->constantes[nci].s; Result val = map_obtener(m->exports, n); if(val.tipo==TIPO_NULO) lanzar_error(ERROR_SINTAXIS, "No en mod"); int vi=hash_lookup(&ht_vars, n); if(vi==-1) vi=n_v++; strcpy(vars[vi].nombre, n); vars[vi].tipo=val.tipo; vars[vi].obj=val.obj; vars[vi].mut = MUT_VAR; if(val.tipo==TIPO_NUMERO) vars[vi].val_num=val.n; else if(val.tipo==TIPO_CADENA) vars[vi].val_str=val.s; hash_insert(&ht_vars, vars[vi].nombre, vi); }
        DISPATCH();
    }
    CASE(OP_LANZAR_ERROR) { uint8_t t = *ip++; lanzar_error(t, "Error lanzado desde bytecode"); DISPATCH(); }
    CASE(OP_DEFINIR_CON_TIPO) {
        uint8_t ni = *ip++; TipoExacto te = (TipoExacto)(*ip++); Result n = chunk->constantes[ni]; Result v = extraer(); aplicar_limites_tipo(&v, te);
        int vi = hash_lookup(&ht_vars, n.s);
        if (vi == -1) {
            vi = n_v++; strcpy(vars[vi].nombre, n.s); hash_insert(&ht_vars, vars[vi].nombre, vi); vars[vi].es_publico = vm.export_proximo; vm.export_proximo = false;
        } else {
            if (vars[vi].mut != MUT_VAR || vm.mut_proximo != MUT_VAR) {
                lanzar_error(ERROR_TIPO, "Variable inmutable");
            }
            liberar_variable(&vars[vi]);
        }
        vars[vi].tipo_ex=te; vars[vi].tipo=v.tipo; vars[vi].obj=v.obj; if(v.tipo==TIPO_CADENA) vars[vi].val_str=v.s; else vars[vi].val_num=v.n;
        vars[vi].mut = vm.mut_proximo;
        vm.mut_proximo = MUT_VAR;
        DISPATCH();
    }
    CASE(OP_ENUM) {
        uint8_t ni = *ip++; uint8_t nv = *ip++; EnumDef* ed = gc_alloc(sizeof(EnumDef), OBJ_ENUM_DEF); strcpy(ed->nombre, chunk->constantes[ni].s); ed->n_variantes = nv;
        for(int i=0; i<nv; i++) { uint8_t vni = *ip++; uint8_t nf = *ip++; strcpy(ed->variantes[i].nombre, chunk->constantes[vni].s); ed->variantes[i].n_campos = nf; }
        int vi = hash_lookup(&ht_vars, ed->nombre); if(vi==-1) { vi=n_v++; strcpy(vars[vi].nombre, ed->nombre); hash_insert(&ht_vars, ed->nombre, vi); }
        vars[vi].tipo=TIPO_ENUM; vars[vi].enum_def=ed; vars[vi].obj=(Obj*)ed; vars[vi].mut = MUT_VAR; vars[vi].es_publico=vm.export_proximo; vm.export_proximo=false; DISPATCH();
    }
    CASE(OP_ENUM_INSTANCIA) {
        uint8_t vi = *ip++; uint8_t na = *ip++; Result er = extraer(); if(er.tipo!=TIPO_ENUM) lanzar_error(ERROR_TIPO, "Enum req");
        EnumValor* ev = gc_alloc(sizeof(EnumValor), OBJ_ENUM_VAL); ev->definicion=er.enum_def; ev->variante_index=vi; ev->valores=na?malloc(sizeof(Result)*na):NULL; for(int i=na-1; i>=0; i--) ev->valores[i]=extraer();
        empujar((Result){.tipo=TIPO_ENUM, .enum_val=ev, .obj=(Obj*)ev}); DISPATCH();
    }
    CASE(OP_ES_TIPO) {
        Result b = extraer(); Result a = extraer(); bool match = false;
        if (b.tipo == TIPO_CLASE && a.tipo == TIPO_INSTANCIA) {
            Clase* target = &clases[b.clase_index]; Clase* curr = a.inst->clase;
            while (curr) { if (curr == target) { match = true; break; } curr = curr->clase_padre; }
        } else if (b.tipo == TIPO_ENUM && a.tipo == TIPO_ENUM) {
            if (a.enum_val && b.enum_def && (uintptr_t)b.enum_val > 0 && (uintptr_t)b.enum_val < 256) {
                int target_var = (int)(uintptr_t)b.enum_val - 1; match = (a.enum_val->definicion == b.enum_def && a.enum_val->variante_index == target_var);
            } else if (a.enum_val && b.enum_val) { match = (a.enum_val->definicion == b.enum_val->definicion && a.enum_val->variante_index == b.enum_val->variante_index); }
            else if (a.enum_val && b.enum_def && !b.enum_val) { match = (a.enum_val->definicion == b.enum_def); }
        } else if (b.tipo == TIPO_FUNCION) {
            const char* name = funcs[b.func_index].nombre;
            if (!strcmp(name, "int") && a.tipo == TIPO_NUMERO) match = true;
            else if (!strcmp(name, "string") && a.tipo == TIPO_CADENA) match = true;
            else if (!strcmp(name, "bool") || !strcmp(name, "booleano")) {
                if (a.tipo == TIPO_BOOL) match = true;
                else if (a.tipo == TIPO_NUMERO && (a.n == 0 || a.n == 1)) match = true;
            }
            else if (!strcmp(name, "array") && a.tipo == TIPO_ARRAY) match = true;
            else if (!strcmp(name, "mapa") && a.tipo == TIPO_MAP) match = true;
        } else if (b.tipo == a.tipo) match = true;
        empujar(res_num(match ? 1 : 0)); DISPATCH();
    }
    CASE(OP_TRY_BEGIN) {
        uint16_t co = (*ip++ << 8); co |= *ip++; uint16_t fo = (*ip++ << 8); fo |= *ip++;
        if (vm.exception_sp < MAX_TRY_DEPTH) { vm.exception_stack[vm.exception_sp].catch_ip = co?(chunk->codigo+co):NULL; vm.exception_stack[vm.exception_sp].finally_ip = fo?(chunk->codigo+fo):NULL; vm.exception_stack[vm.exception_sp].pila_sp = (int)(vm.pila_tope-vm.pila); vm.exception_sp++; }
        DISPATCH();
    }
    CASE(OP_TRY_END) { if (vm.exception_sp > 0) vm.exception_sp--; DISPATCH(); }
    CASE(OP_LANZAR) { Result e = extraer(); vm.hay_error_pendiente = false; vm_unwind_to_handler(e); ip = vm.ip; DISPATCH(); }
    CASE(OP_FINALLY_END) { if (vm.hay_error_pendiente) { Result err = vm.error_pendiente; vm.hay_error_pendiente = false; vm_unwind_to_handler(err); } DISPATCH(); }
    CASE(OP_MARCAR_EXPORT) { vm.export_proximo = true; DISPATCH(); }
    CASE(OP_MARCAR_MUT) { uint8_t m = *ip++; vm.mut_proximo = m; DISPATCH(); }
    CASE(OP_NOP) { DISPATCH(); }

#ifndef __GNUC__
    default: lanzar_error(ERROR_SINTAXIS, "Opcode desconocido %d", opcode);
    }
}
#endif
    vm.ip = ip_previo;
    vm.chunk = chunk_previo;
    return (Result){.tipo = TIPO_NULO};
}

void vm_unwind_to_handler(Result error) {
    while (vm.exception_sp > 0) {
        vm.exception_sp--;
        uint8_t* catch_ip = vm.exception_stack[vm.exception_sp].catch_ip;
        uint8_t* finally_ip = vm.exception_stack[vm.exception_sp].finally_ip;
        if (catch_ip) { vm.pila_tope = vm.pila + vm.exception_stack[vm.exception_sp].pila_sp; empujar(error); vm.hay_error_pendiente = false; vm.ip = catch_ip; longjmp(vm.recover_jmp, 1); }
        if (finally_ip) { vm.pila_tope = vm.pila + vm.exception_stack[vm.exception_sp].pila_sp; vm.hay_error_pendiente = true; vm.error_pendiente = error; vm.ip = finally_ip; longjmp(vm.recover_jmp, 1); }
    }
    printf("ERROR NO CAPTURADO: "); imprimir_result(error); printf("\n"); exit(1);
}

static ObjModulo* cargar_modulo(const char* path_orig) {
    Result match = map_obtener(vm.modulos_cargados, path_orig);
    if (match.tipo == TIPO_MODULO) return (ObjModulo*)match.obj;
    #ifndef ITSUKI_BUNDLED
    ObjModulo* native = itsuki_ext_cargar_modulo_nativo(path_orig);
    if (native) {
        Result mod_res = {.tipo = TIPO_MODULO, .obj = (Obj*)native};
        map_establecer(vm.modulos_cargados, path_orig, mod_res);
        if (strcmp(path_orig, native->path) != 0) {
            map_establecer(vm.modulos_cargados, native->path, mod_res);
        }
        return native;
    }
    #endif
    char* src = NULL; const char* path = path_orig;
    char p2[256];
    #ifdef ITSUKI_BUNDLED
    for(int i=0; bundled_files[i].path; i++) {
        if(!strcmp(bundled_files[i].path, path_orig)) {
            src = my_strdup(bundled_files[i].content);
            break;
        }
        char p_ext[256];
        sprintf(p_ext, "%s.suki", path_orig);
        if(!strcmp(bundled_files[i].path, p_ext)) {
            src = my_strdup(bundled_files[i].content);
            path = bundled_files[i].path;
            break;
        }
    }
    #endif
    if (!src) {
        FILE* file = fopen(path, "rb");
        if (!file) {
            if (!strstr(path, ".suki")) {
                sprintf(p2, "%s.suki", path);
                file = fopen(p2, "rb");
                if (!file) lanzar_error(ERROR_ARCHIVO, "No se pudo abrir el modulo '%s'", path);
                path = p2;
            } else lanzar_error(ERROR_ARCHIVO, "No se pudo abrir el modulo '%s'", path);
        }
        fseek(file, 0, SEEK_END);
        long sz = ftell(file);
        fseek(file, 0, SEEK_SET);
        src = malloc(sz + 1);
        if (fread(src, 1, sz, file) != (size_t)sz) {
            free(src);
            fclose(file);
            lanzar_error(ERROR_ARCHIVO, "No se pudo leer el modulo '%s'", path);
        }
        src[sz] = 0;
        fclose(file);
    }
    ObjModulo* mod = (ObjModulo*)gc_alloc(sizeof(ObjModulo), OBJ_MODULO);
    strncpy(mod->path, path, 255);
    mod->path[255] = '\0';
    mod->exports = map_crear();
    map_establecer(vm.modulos_cargados, path_orig, (Result){.tipo = TIPO_MODULO, .obj = (Obj*)mod});
    int n_v_old = n_v; int n_f_old = n_f; extern TokenStream* ts; extern int ln; TokenStream* old_ts = ts; Token old_tk = tk; int old_ln = ln;
    ts = tokenize_all(src); if (ts) { adv(); while (tk.tipo != TOKEN_EOF) { NodoAST* node = parse_stmt(); if (node) { Chunk* ch = compilar_a_bytecode(node); vm_ejecutar(ch); free_chunk(ch); free(ch); free_ast(node); } } free_token_stream(ts); }
    ts = old_ts; tk = old_tk; ln = old_ln; free(src);
    for (int i = n_v_old; i < n_v; i++) if (vars[i].es_publico) map_establecer(mod->exports, vars[i].nombre, result_desde_variable(&vars[i]));
    for (int i = n_f_old; i < n_f; i++) if (funcs[i].es_publico) map_establecer(mod->exports, funcs[i].nombre, (Result){.tipo = TIPO_FUNCION, .func_index = i});
    for (int i = 0; i < n_clases; i++) if (clases[i].es_publico) map_establecer(mod->exports, clases[i].nombre, (Result){.tipo = TIPO_CLASE, .clase_index = i});
    return mod;
}
