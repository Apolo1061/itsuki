#include "itsuki.h"

#define ENABLE_SUPERINS 1
#define ENABLE_OPTIMIZER 1

void init_chunk(Chunk* chunk) {
    chunk->capacidad = 0;
    chunk->contador = 0;
    chunk->codigo = NULL;
    chunk->constantes = NULL;
    chunk->capacidad_constantes = 0;
    chunk->contador_constantes = 0;
    chunk->cache_global_idx = NULL;
    chunk->cache_global_ver = NULL;
    chunk->cache_size = 0;
    chunk->cache_prop_cls = NULL;
    chunk->cache_prop_name = NULL;
    chunk->cache_prop_index = NULL;
    chunk->cache_prop_kind = NULL;
    chunk->cache_invoke_cls = NULL;
    chunk->cache_invoke_func = NULL;
    chunk->cache_invoke_kind = NULL;
}

void free_chunk(Chunk* chunk) {
    if (chunk->codigo) free(chunk->codigo);
    if (chunk->cache_global_idx) free(chunk->cache_global_idx);
    if (chunk->cache_global_ver) free(chunk->cache_global_ver);
    if (chunk->cache_prop_cls) free(chunk->cache_prop_cls);
    if (chunk->cache_prop_name) free(chunk->cache_prop_name);
    if (chunk->cache_prop_index) free(chunk->cache_prop_index);
    if (chunk->cache_prop_kind) free(chunk->cache_prop_kind);
    if (chunk->cache_invoke_cls) free(chunk->cache_invoke_cls);
    if (chunk->cache_invoke_func) free(chunk->cache_invoke_func);
    if (chunk->cache_invoke_kind) free(chunk->cache_invoke_kind);
    for (int i = 0; i < chunk->contador_constantes; i++) {
        liberar_resultado(chunk->constantes[i]);
    }
    if (chunk->constantes) free(chunk->constantes);
    init_chunk(chunk);
}

void escribir_chunk(Chunk* chunk, uint8_t byte) {
    if (chunk->capacidad < chunk->contador + 1) {
        int vieja_capacidad = chunk->capacidad;
        chunk->capacidad = (vieja_capacidad < 8) ? 8 : vieja_capacidad * 2;
        chunk->codigo = realloc(chunk->codigo, chunk->capacidad);
    }
    chunk->codigo[chunk->contador++] = byte;
}

int agregar_constante(Chunk* chunk, Result valor) {
    if (chunk->capacidad_constantes < chunk->contador_constantes + 1) {
        int vieja_capacidad = chunk->capacidad_constantes;
        chunk->capacidad_constantes = (vieja_capacidad < 8) ? 8 : vieja_capacidad * 2;
        chunk->constantes = realloc(chunk->constantes, sizeof(Result) * chunk->capacidad_constantes);
    }
    chunk->constantes[chunk->contador_constantes++] = valor;
    return chunk->contador_constantes - 1;
}

typedef struct CompilerContext {
    struct CompilerContext* padre;
    char locales[64][MAX_ID_LEN];
    int n_locales;
    struct {
        char nombre[64];
        bool es_local;
    } upvalues[64];
    int n_upvalues;
} CompilerContext;

typedef struct {
    int continue_target;
    int break_jumps[256];
    int n_breaks;
    int continue_jumps[256];
    int n_continues;
} LoopContext;

static LoopContext loop_stack[64];
static int loop_depth = 0;

static bool obtener_valor_constante(NodoAST* n, double* out) {
    if (!n) return false;
    if (n->tipo == AST_NUMERO) {
        *out = n->datos.numero;
        return true;
    }
    if (n->tipo == AST_UNOP) {
        double v;
        if (obtener_valor_constante(n->datos.unop.operando, &v)) {
            if (n->datos.unop.operador == TOKEN_MENOS) { *out = -v; return true; }
            if (n->datos.unop.operador == TOKEN_NO) { *out = (v == 0) ? 1 : 0; return true; }
        }
    }
    if (n->tipo == AST_BINOP) {
        double v1, v2;
        if (obtener_valor_constante(n->datos.binop.izquierda, &v1) && 
            obtener_valor_constante(n->datos.binop.derecha, &v2)) {
            switch (n->datos.binop.operador) {
                case TOKEN_MAS: *out = v1 + v2; return true;
                case TOKEN_MENOS: *out = v1 - v2; return true;
                case TOKEN_MULT: *out = v1 * v2; return true;
                case TOKEN_DIV: if (v2 != 0) { *out = v1 / v2; return true; } break;
            }
        }
    }
    return false;
}

static bool es_constexpr(NodoAST* n) {
    if (!n) return false;
    if (n->tipo == AST_NUMERO || n->tipo == AST_CADENA || n->tipo == AST_NULO) return true;
    if (n->tipo == AST_UNOP) {
        double v;
        return obtener_valor_constante(n, &v);
    }
    if (n->tipo == AST_BINOP) {
        double v;
        return obtener_valor_constante(n, &v);
    }
    return false;
}

static void preparar_cache_chunk(Chunk* chunk) {
    if (!chunk || chunk->contador <= 0) return;
    if (chunk->cache_global_idx) free(chunk->cache_global_idx);
    if (chunk->cache_global_ver) free(chunk->cache_global_ver);
    if (chunk->cache_prop_cls) free(chunk->cache_prop_cls);
    if (chunk->cache_prop_name) free(chunk->cache_prop_name);
    if (chunk->cache_prop_index) free(chunk->cache_prop_index);
    if (chunk->cache_prop_kind) free(chunk->cache_prop_kind);
    if (chunk->cache_invoke_cls) free(chunk->cache_invoke_cls);
    if (chunk->cache_invoke_func) free(chunk->cache_invoke_func);
    if (chunk->cache_invoke_kind) free(chunk->cache_invoke_kind);
    chunk->cache_size = chunk->contador;
    chunk->cache_global_idx = malloc(sizeof(int) * chunk->cache_size);
    chunk->cache_global_ver = malloc(sizeof(uint32_t) * chunk->cache_size);
    chunk->cache_prop_cls = malloc(sizeof(Clase*) * chunk->cache_size);
    chunk->cache_prop_name = malloc(sizeof(char*) * chunk->cache_size);
    chunk->cache_prop_index = malloc(sizeof(int) * chunk->cache_size);
    chunk->cache_prop_kind = malloc(sizeof(uint8_t) * chunk->cache_size);
    chunk->cache_invoke_cls = malloc(sizeof(Clase*) * chunk->cache_size);
    chunk->cache_invoke_func = malloc(sizeof(int) * chunk->cache_size);
    chunk->cache_invoke_kind = malloc(sizeof(uint8_t) * chunk->cache_size);
    for (int i = 0; i < chunk->cache_size; i++) {
        chunk->cache_global_idx[i] = -1;
        chunk->cache_global_ver[i] = 0;
        chunk->cache_prop_cls[i] = NULL;
        chunk->cache_prop_name[i] = NULL;
        chunk->cache_prop_index[i] = -1;
        chunk->cache_prop_kind[i] = 0;
        chunk->cache_invoke_cls[i] = NULL;
        chunk->cache_invoke_func[i] = -1;
        chunk->cache_invoke_kind[i] = 0;
    }
}

static int leer_u16(const uint8_t* code, int off) {
    return (int)((code[off] << 8) | code[off + 1]);
}

static void escribir_u16(uint8_t* code, int off, int val) {
    code[off] = (uint8_t)((val >> 8) & 0xff);
    code[off + 1] = (uint8_t)(val & 0xff);
}

static int instr_len_at(Chunk* chunk, int off) {
    if (!chunk || off < 0 || off >= chunk->contador) return 0;
    uint8_t op = chunk->codigo[off];
    switch (op) {
        case OP_CONSTANTE:
        case OP_DEFINIR_GLOBAL:
        case OP_OBTENER_GLOBAL:
        case OP_ESTABLECER_GLOBAL:
        case OP_OBTENER_LOCAL:
        case OP_ESTABLECER_LOCAL:
        case OP_LLAMAR:
        case OP_COLA_LLAMAR:
        case OP_PRINT:
        case OP_OBTENER_UPVALUE:
        case OP_ESTABLECER_UPVALUE:
        case OP_ESTABLECER_PROPIEDAD:
        case OP_LANZAR_ERROR:
        case OP_CONST_SUMA:
        case OP_CONST_RESTA:
        case OP_CONST_MULT:
        case OP_CONST_DIV:
        case OP_MARCAR_MUT:
            return 2;
        case OP_SALTAR:
        case OP_SALTAR_SI_FALSO:
        case OP_DEFINIR_CON_TIPO:
        case OP_ENUM_INSTANCIA:
        case OP_INVOQUE_METODO:
        case OP_SUPER:
        case OP_CLOSURE:
        case OP_NUEVA:
            return 3;
        case OP_TRY_BEGIN:
            return 5;
        case OP_IMPORTAR: {
            if (off + 3 >= chunk->contador) return 1;
            uint8_t nn = chunk->codigo[off + 3];
            return 1 + 3 + nn;
        }
        case OP_CLASE: {
            if (off + 3 >= chunk->contador) return 1;
            uint8_t nm = chunk->codigo[off + 3];
            return 1 + 3 + (nm * 2);
        }
        case OP_ENUM: {
            if (off + 2 >= chunk->contador) return 1;
            uint8_t nv = chunk->codigo[off + 2];
            return 1 + 2 + (nv * 2);
        }
        default:
            return 1;
    }
}

typedef struct {
    int out_pos;
    int target_old;
} JumpPatch;

static int threaded_target(Chunk* chunk, int* off_to_idx, bool* reach, int* next_reach, int instr_count, int target) {
    int guard = 0;
    while (target >= 0 && target < chunk->contador && guard++ < instr_count) {
        int idx = off_to_idx[target];
        if (idx < 0 || !reach[idx]) break;
        uint8_t op = chunk->codigo[target];
        if (op == OP_NOP) {
            int next = next_reach[idx];
            if (next == target) break;
            target = next;
            continue;
        }
        if (op == OP_SALTAR) {
            int t2 = leer_u16(chunk->codigo, target + 1);
            if (t2 == target) break;
            target = t2;
            continue;
        }
        break;
    }
    return target;
}

static void optimizar_chunk(Chunk* chunk) {
    if (!chunk || !chunk->codigo) return;

    int n = chunk->contador;
    int max_instr = n > 0 ? n : 1;
    int* offs = malloc(sizeof(int) * max_instr);
    int* lens = malloc(sizeof(int) * max_instr);
    uint8_t* ops = malloc(sizeof(uint8_t) * max_instr);
    int* off_to_idx = malloc(sizeof(int) * (n + 1));
    bool* reach = malloc(sizeof(bool) * max_instr);
    int* next_reach = malloc(sizeof(int) * max_instr);

    for (int i = 0; i <= n; i++) off_to_idx[i] = -1;

    int instr_count = 0;
    bool invalid = false;
    for (int off = 0; off < n;) {
        int len = instr_len_at(chunk, off);
        if (len <= 0 || off + len > n) { invalid = true; break; }
        offs[instr_count] = off;
        lens[instr_count] = len;
        ops[instr_count] = chunk->codigo[off];
        off_to_idx[off] = instr_count;
        instr_count++;
        off += len;
    }

    if (invalid || instr_count == 0) {
        free(offs);
        free(lens);
        free(ops);
        free(off_to_idx);
        free(reach);
        free(next_reach);
        return;
    }

    for (int i = 0; i < instr_count; i++) reach[i] = false;

    int stack_cap = instr_count > 0 ? instr_count : 1;
    int* stack = malloc(sizeof(int) * stack_cap);
    int sp = 0;
    if (instr_count > 0) stack[sp++] = 0;

    while (sp > 0) {
        int off = stack[--sp];
        if (off < 0 || off >= n) continue;
        int idx = off_to_idx[off];
        if (idx < 0 || reach[idx]) continue;
        reach[idx] = true;
        uint8_t op = ops[idx];
        int len = lens[idx];
        int fall = off + len;
        if (op == OP_SALTAR) {
            int target = leer_u16(chunk->codigo, off + 1);
            if (sp + 1 > stack_cap) { stack_cap *= 2; stack = realloc(stack, sizeof(int) * stack_cap); }
            stack[sp++] = target;
        } else if (op == OP_SALTAR_SI_FALSO) {
            int target = leer_u16(chunk->codigo, off + 1);
            if (sp + 2 > stack_cap) { stack_cap *= 2; stack = realloc(stack, sizeof(int) * stack_cap); }
            stack[sp++] = target;
            if (fall < n) stack[sp++] = fall;
        } else if (op == OP_TRY_BEGIN) {
            int catch_off = leer_u16(chunk->codigo, off + 1);
            int fin_off = leer_u16(chunk->codigo, off + 3);
            if (sp + 3 > stack_cap) { stack_cap *= 2; stack = realloc(stack, sizeof(int) * stack_cap); }
            if (fall < n) stack[sp++] = fall;
            if (catch_off != 0) stack[sp++] = catch_off;
            if (fin_off != 0) stack[sp++] = fin_off;
        } else if (op == OP_RETORNAR || op == OP_LANZAR || op == OP_LANZAR_ERROR) {
            continue;
        } else {
            if (fall < n) {
                if (sp + 1 > stack_cap) { stack_cap *= 2; stack = realloc(stack, sizeof(int) * stack_cap); }
                stack[sp++] = fall;
            }
        }
    }

    int* pred_count = malloc(sizeof(int) * instr_count);
    for (int i = 0; i < instr_count; i++) pred_count[i] = 0;
    for (int i = 0; i < instr_count; i++) {
        if (!reach[i]) continue;
        int off = offs[i];
        uint8_t op = ops[i];
        int len = lens[i];
        int fall = off + len;
        if (op == OP_SALTAR) {
            int target = leer_u16(chunk->codigo, off + 1);
            int ti = (target >= 0 && target < n) ? off_to_idx[target] : -1;
            if (ti >= 0) pred_count[ti]++;
        } else if (op == OP_SALTAR_SI_FALSO) {
            int target = leer_u16(chunk->codigo, off + 1);
            int ti = (target >= 0 && target < n) ? off_to_idx[target] : -1;
            if (ti >= 0) pred_count[ti]++;
            if (fall < n) {
                int fi = off_to_idx[fall];
                if (fi >= 0) pred_count[fi]++;
            }
        } else if (op == OP_TRY_BEGIN) {
            int catch_off = leer_u16(chunk->codigo, off + 1);
            int fin_off = leer_u16(chunk->codigo, off + 3);
            if (fall < n) {
                int fi = off_to_idx[fall];
                if (fi >= 0) pred_count[fi]++;
            }
            if (catch_off != 0) {
                int ci = off_to_idx[catch_off];
                if (ci >= 0) pred_count[ci]++;
            }
            if (fin_off != 0) {
                int fi2 = off_to_idx[fin_off];
                if (fi2 >= 0) pred_count[fi2]++;
            }
        } else if (op == OP_RETORNAR || op == OP_LANZAR || op == OP_LANZAR_ERROR) {
            continue;
        } else {
            if (fall < n) {
                int fi = off_to_idx[fall];
                if (fi >= 0) pred_count[fi]++;
            }
        }
    }

    int next = n;
    for (int i = instr_count - 1; i >= 0; i--) {
        next_reach[i] = next;
        if (reach[i]) next = offs[i];
    }

    int* old_to_new = malloc(sizeof(int) * (n + 1));
    for (int i = 0; i <= n; i++) old_to_new[i] = -1;

    uint8_t* new_code = malloc(n > 0 ? n : 1);
    int new_cap = n > 0 ? n : 1;
    int new_len = 0;

    JumpPatch* patches = NULL;
    int n_patches = 0;
    int cap_patches = 0;

    bool* skip = malloc(sizeof(bool) * instr_count);
    for (int i = 0; i < instr_count; i++) skip[i] = false;

    for (int i = 0; i < instr_count; i++) {
        if (!reach[i]) continue;
        if (skip[i]) continue;
        int off = offs[i];
        uint8_t op = ops[i];
        int len = lens[i];

        if (op == OP_NOP) continue;

        if ((op == OP_CONSTANTE || op == OP_NULO || op == OP_VERDADERO || op == OP_FALSO) && i + 1 < instr_count) {
            if (reach[i + 1] && ops[i + 1] == OP_POP && (offs[i] + len == offs[i + 1])) {
                bool prev_fall = (i > 0 && reach[i - 1] && (offs[i - 1] + lens[i - 1] == offs[i]));
                bool ok_pred_i = (pred_count[i] == 0 && !prev_fall) || (pred_count[i] == 1 && prev_fall);
                if (ok_pred_i && pred_count[i + 1] == 1) {
                    skip[i + 1] = true;
                    continue;
                }
            }
        }

        if (ENABLE_SUPERINS && op == OP_CONSTANTE && i + 1 < instr_count) {
            if (reach[i + 1] && !skip[i + 1] && (offs[i] + len == offs[i + 1])) {
                bool prev_fall = (i > 0 && reach[i - 1] && (offs[i - 1] + lens[i - 1] == offs[i]));
                if (prev_fall && pred_count[i + 1] == 1) {
                    uint8_t op2 = ops[i + 1];
                    uint8_t newop = 0;
                    if (op2 == OP_SUMA) newop = OP_CONST_SUMA;
                    if (newop != 0) {
                        old_to_new[off] = new_len;
                        if (new_len + 2 > new_cap) { new_cap = (new_cap * 2) + 8; new_code = realloc(new_code, new_cap); }
                        new_code[new_len++] = newop;
                        new_code[new_len++] = chunk->codigo[off + 1];
                        skip[i + 1] = true;
                        continue;
                    }
                }
            }
        }

        if (op == OP_SALTAR) {
            int target = leer_u16(chunk->codigo, off + 1);
            target = threaded_target(chunk, off_to_idx, reach, next_reach, instr_count, target);
            int fall = next_reach[i];
            if (target == fall) continue;
            old_to_new[off] = new_len;
            if (new_len + 3 > new_cap) { new_cap = (new_cap * 2) + 8; new_code = realloc(new_code, new_cap); }
            new_code[new_len++] = OP_SALTAR;
            new_code[new_len++] = 0;
            new_code[new_len++] = 0;
            if (n_patches >= cap_patches) { cap_patches = cap_patches ? cap_patches * 2 : 8; patches = realloc(patches, sizeof(JumpPatch) * cap_patches); }
            patches[n_patches++] = (JumpPatch){ .out_pos = new_len - 2, .target_old = target };
            continue;
        }

        if (op == OP_SALTAR_SI_FALSO) {
            int target = leer_u16(chunk->codigo, off + 1);
            target = threaded_target(chunk, off_to_idx, reach, next_reach, instr_count, target);
            int fall = next_reach[i];
            if (target == fall) {
                old_to_new[off] = new_len;
                if (new_len + 1 > new_cap) { new_cap = (new_cap * 2) + 8; new_code = realloc(new_code, new_cap); }
                new_code[new_len++] = OP_POP;
                continue;
            }
            old_to_new[off] = new_len;
            if (new_len + 3 > new_cap) { new_cap = (new_cap * 2) + 8; new_code = realloc(new_code, new_cap); }
            new_code[new_len++] = OP_SALTAR_SI_FALSO;
            new_code[new_len++] = 0;
            new_code[new_len++] = 0;
            if (n_patches >= cap_patches) { cap_patches = cap_patches ? cap_patches * 2 : 8; patches = realloc(patches, sizeof(JumpPatch) * cap_patches); }
            patches[n_patches++] = (JumpPatch){ .out_pos = new_len - 2, .target_old = target };
            continue;
        }

        if (op == OP_TRY_BEGIN) {
            int catch_off = leer_u16(chunk->codigo, off + 1);
            int fin_off = leer_u16(chunk->codigo, off + 3);
            if (catch_off != 0) catch_off = threaded_target(chunk, off_to_idx, reach, next_reach, instr_count, catch_off);
            if (fin_off != 0) fin_off = threaded_target(chunk, off_to_idx, reach, next_reach, instr_count, fin_off);
            old_to_new[off] = new_len;
            if (new_len + 5 > new_cap) { new_cap = (new_cap * 2) + 8; new_code = realloc(new_code, new_cap); }
            new_code[new_len++] = OP_TRY_BEGIN;
            new_code[new_len++] = 0; new_code[new_len++] = 0;
            new_code[new_len++] = 0; new_code[new_len++] = 0;
            if (catch_off != 0) {
                if (n_patches >= cap_patches) { cap_patches = cap_patches ? cap_patches * 2 : 8; patches = realloc(patches, sizeof(JumpPatch) * cap_patches); }
                patches[n_patches++] = (JumpPatch){ .out_pos = new_len - 4, .target_old = catch_off };
            }
            if (fin_off != 0) {
                if (n_patches >= cap_patches) { cap_patches = cap_patches ? cap_patches * 2 : 8; patches = realloc(patches, sizeof(JumpPatch) * cap_patches); }
                patches[n_patches++] = (JumpPatch){ .out_pos = new_len - 2, .target_old = fin_off };
            }
            continue;
        }

        old_to_new[off] = new_len;
        if (new_len + len > new_cap) { new_cap = (new_cap * 2) + len + 8; new_code = realloc(new_code, new_cap); }
        memcpy(&new_code[new_len], &chunk->codigo[off], len);
        new_len += len;
    }

    old_to_new[n] = new_len;
    for (int i = 0; i < n_patches; i++) {
        int target = patches[i].target_old;
        int new_t = (target == n) ? new_len : old_to_new[target];
        if (new_t < 0) new_t = new_len;
        if (new_t > 0xffff) new_t = 0xffff;
        escribir_u16(new_code, patches[i].out_pos, new_t);
    }

    free(chunk->codigo);
    chunk->codigo = new_code;
    chunk->contador = new_len;
    chunk->capacidad = new_cap;

    free(offs);
    free(lens);
    free(ops);
    free(off_to_idx);
    free(reach);
    free(next_reach);
    free(old_to_new);
    free(stack);
    free(patches);
    free(pred_count);
    free(skip);
}

static void compilar_nodo(Chunk* chunk, NodoAST* n) {
    if (!n) return;

    switch (n->tipo) {
        case AST_NUMERO: {
            Result v = {.tipo = TIPO_NUMERO, .n = n->datos.numero};
            int idx = agregar_constante(chunk, v);
            escribir_chunk(chunk, OP_CONSTANTE);
            escribir_chunk(chunk, (uint8_t)idx);
            break;
        }
        case AST_CADENA: {
            Result v = gc_new_string(n->datos.cadena);
            int idx = agregar_constante(chunk, v);
            escribir_chunk(chunk, OP_CONSTANTE);
            escribir_chunk(chunk, (uint8_t)idx);
            break;
        }
        case AST_NULO:
            escribir_chunk(chunk, OP_NULO);
            break;
            
        case AST_BINOP: {
            double folded_val;
            if (obtener_valor_constante(n, &folded_val)) {
                Result v = {.tipo = TIPO_NUMERO, .n = folded_val};
                int idx = agregar_constante(chunk, v);
                escribir_chunk(chunk, OP_CONSTANTE);
                escribir_chunk(chunk, (uint8_t)idx);
                break;
            }

            compilar_nodo(chunk, n->datos.binop.izquierda);
            compilar_nodo(chunk, n->datos.binop.derecha);
            switch (n->datos.binop.operador) {
                case TOKEN_MAS: escribir_chunk(chunk, OP_SUMA); break;
                case TOKEN_MENOS: escribir_chunk(chunk, OP_RESTA); break;
                case TOKEN_MULT: escribir_chunk(chunk, OP_MULT); break;
                case TOKEN_DIV: escribir_chunk(chunk, OP_DIV); break;
                case TOKEN_MODULO: escribir_chunk(chunk, OP_MODULO); break;
                case TOKEN_IGUAL_IGUAL: escribir_chunk(chunk, OP_IGUAL); break;
                case TOKEN_DIFERENTE: 
                    escribir_chunk(chunk, OP_IGUAL);
                    escribir_chunk(chunk, OP_NO);
                    break;
                case TOKEN_MAYOR: escribir_chunk(chunk, OP_MAYOR); break;
                case TOKEN_MENOR: escribir_chunk(chunk, OP_MENOR); break;
                case TOKEN_ES: escribir_chunk(chunk, OP_ES_TIPO); break;
                case TOKEN_MAYOR_IGUAL:
                    escribir_chunk(chunk, OP_MENOR);
                    escribir_chunk(chunk, OP_NO);
                    break;
                case TOKEN_MENOR_IGUAL:
                    escribir_chunk(chunk, OP_MAYOR);
                    escribir_chunk(chunk, OP_NO);
                    break;
                default: break;
            }
            break;
        }
        
        case AST_UNOP: {
            compilar_nodo(chunk, n->datos.unop.operando);
            if (n->datos.unop.operador == TOKEN_NO) escribir_chunk(chunk, OP_NO);
            else if (n->datos.unop.operador == TOKEN_MENOS) {
                Result cero = {.tipo = TIPO_NUMERO, .n = 0};
                int idx = agregar_constante(chunk, cero);
                escribir_chunk(chunk, OP_CONSTANTE);
                escribir_chunk(chunk, (uint8_t)idx);
            }
            break;
        }

        case AST_ASIGNACION: {
            bool es_decl = n->datos.asignacion.es_declaracion;
            uint8_t mut = n->datos.asignacion.var_mut;
            if (!n->datos.asignacion.valor) {
                if (es_decl && mut != MUT_VAR) {
                    lanzar_error(ERROR_SINTAXIS, "Se esperaba un valor para la declaracion");
                }
                escribir_chunk(chunk, OP_NULO);
            } else {
                if (es_decl && mut == MUT_CONST) {
                    if (!es_constexpr(n->datos.asignacion.valor)) {
                        lanzar_error(ERROR_SINTAXIS, "const requiere valor constante en compilacion");
                    }
                }
                compilar_nodo(chunk, n->datos.asignacion.valor);
            }
            if (n->datos.asignacion.receptor) {
                // Asignacion de propiedad: obj.prop = val
                compilar_nodo(chunk, n->datos.asignacion.receptor);
                Result prop = gc_new_string(n->datos.asignacion.nombre);
                int idx = agregar_constante(chunk, prop);
                escribir_chunk(chunk, OP_ESTABLECER_PROPIEDAD);
                escribir_chunk(chunk, (uint8_t)idx);
            } else {
                if (n->es_publico) escribir_chunk(chunk, OP_MARCAR_EXPORT);
                if (es_decl && mut != MUT_VAR) {
                    escribir_chunk(chunk, OP_MARCAR_MUT);
                    escribir_chunk(chunk, mut);
                }
                Result nombre = gc_new_string(n->datos.asignacion.nombre);
                int idx = agregar_constante(chunk, nombre);
                if (n->tipo_ex != TEX_AUTO) {
                    escribir_chunk(chunk, OP_DEFINIR_CON_TIPO);
                    escribir_chunk(chunk, (uint8_t)idx);
                    escribir_chunk(chunk, (uint8_t)n->tipo_ex);
                } else {
                    escribir_chunk(chunk, OP_DEFINIR_GLOBAL);
                    escribir_chunk(chunk, (uint8_t)idx);
                }
            }
            break;
        }

        case AST_IDENTIFICADOR: {
            Result nombre = gc_new_string(n->datos.identificador);
            int idx = agregar_constante(chunk, nombre);
            escribir_chunk(chunk, OP_OBTENER_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx);
            break;
        }

        case AST_BLOQUE: {
            for (int i = 0; i < n->datos.bloque.count; i++) {
                compilar_nodo(chunk, n->datos.bloque.sentencias[i]);
            }
            break;
        }

        case AST_MIENTRAS: {
            int inicio_bucle = chunk->contador;
            compilar_nodo(chunk, n->datos.mientras.condicion);
            
            int salto_fin = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR_SI_FALSO);
            escribir_chunk(chunk, 0xff);
            escribir_chunk(chunk, 0xff);
            
            compilar_nodo(chunk, n->datos.mientras.cuerpo);
            
            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, (inicio_bucle >> 8) & 0xff);
            escribir_chunk(chunk, inicio_bucle & 0xff);
            
            int destino_fin = chunk->contador;
            chunk->codigo[salto_fin + 1] = (destino_fin >> 8) & 0xff;
            chunk->codigo[salto_fin + 2] = destino_fin & 0xff;
            break;
        }

        case AST_PARA: {
            compilar_nodo(chunk, n->datos.para.inicio);
            Result nombre = gc_new_string(n->datos.para.var_nombre);
            int idx_var = agregar_constante(chunk, nombre);
            escribir_chunk(chunk, OP_DEFINIR_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx_var);

            int inicio_bucle = chunk->contador;
            
            escribir_chunk(chunk, OP_OBTENER_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx_var);
            compilar_nodo(chunk, n->datos.para.fin);
            escribir_chunk(chunk, OP_MENOR);
            
            int salto_fin = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR_SI_FALSO);
            escribir_chunk(chunk, 0xff);
            escribir_chunk(chunk, 0xff);
            
            compilar_nodo(chunk, n->datos.para.cuerpo);
            
            escribir_chunk(chunk, OP_OBTENER_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx_var);
            if (n->datos.para.paso) compilar_nodo(chunk, n->datos.para.paso);
            else {
                Result uno = {0};
                uno.tipo = TIPO_NUMERO;
                uno.n = 1;
                int idx_uno = agregar_constante(chunk, uno);
                escribir_chunk(chunk, OP_CONSTANTE);
                escribir_chunk(chunk, (uint8_t)idx_uno);
            }
            escribir_chunk(chunk, OP_SUMA);
            escribir_chunk(chunk, OP_DEFINIR_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx_var);
            
            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, (inicio_bucle >> 8) & 0xff);
            escribir_chunk(chunk, inicio_bucle & 0xff);
            
            int destino_fin = chunk->contador;
            chunk->codigo[salto_fin + 1] = (destino_fin >> 8) & 0xff;
            chunk->codigo[salto_fin + 2] = destino_fin & 0xff;
            break;
        }
        case AST_SI: {
            compilar_nodo(chunk, n->datos.si.condicion);
            
            int salto_si_falso = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR_SI_FALSO);
            escribir_chunk(chunk, 0xff);
            escribir_chunk(chunk, 0xff);
            
            compilar_nodo(chunk, n->datos.si.bloque_si);
            
            int salto_sino = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, 0xff);
            escribir_chunk(chunk, 0xff);
            
            int destino = chunk->contador;
            chunk->codigo[salto_si_falso + 1] = (destino >> 8) & 0xff;
            chunk->codigo[salto_si_falso + 2] = destino & 0xff;
            
            if (n->datos.si.bloque_sino) {
                compilar_nodo(chunk, n->datos.si.bloque_sino);
            }
            
            destino = chunk->contador;
            chunk->codigo[salto_sino + 1] = (destino >> 8) & 0xff;
            chunk->codigo[salto_sino + 2] = destino & 0xff;
            break;
        }

        case AST_TRY_CATCH: {
            escribir_chunk(chunk, OP_TRY_BEGIN);
            int try_begin_placeholder = chunk->contador;
            escribir_chunk(chunk, 0); escribir_chunk(chunk, 0);
            escribir_chunk(chunk, 0); escribir_chunk(chunk, 0);

            compilar_nodo(chunk, n->datos.try_catch.bloque_try);
            escribir_chunk(chunk, OP_TRY_END);

            int salto_exito_try = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, 0xff); escribir_chunk(chunk, 0xff);

            int catch_start = chunk->contador;
            if (n->datos.try_catch.bloque_catch) {
                chunk->codigo[try_begin_placeholder] = (catch_start >> 8) & 0xff;
                chunk->codigo[try_begin_placeholder + 1] = catch_start & 0xff;

                if (n->datos.try_catch.var_error[0] != 0) {
                    Result err_name = gc_new_string(n->datos.try_catch.var_error);
                    int idx_err = agregar_constante(chunk, err_name);
                    escribir_chunk(chunk, OP_DEFINIR_GLOBAL); 
                    escribir_chunk(chunk, (uint8_t)idx_err);
                }
                compilar_nodo(chunk, n->datos.try_catch.bloque_catch);
            }

            int salto_post_catch = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, 0xff); escribir_chunk(chunk, 0xff);

            int finally_start = chunk->contador;
            if (n->datos.try_catch.bloque_finally) {
                chunk->codigo[try_begin_placeholder + 2] = (finally_start >> 8) & 0xff;
                chunk->codigo[try_begin_placeholder + 3] = finally_start & 0xff;
                compilar_nodo(chunk, n->datos.try_catch.bloque_finally);
            } else {
                chunk->codigo[try_begin_placeholder + 2] = 0;
                chunk->codigo[try_begin_placeholder + 3] = 0;
            }

            escribir_chunk(chunk, OP_FINALLY_END);
            
            chunk->codigo[salto_exito_try + 1] = (finally_start >> 8) & 0xff;
            chunk->codigo[salto_exito_try + 2] = finally_start & 0xff;
            
            chunk->codigo[salto_post_catch + 1] = (finally_start >> 8) & 0xff;
            chunk->codigo[salto_post_catch + 2] = finally_start & 0xff;
            break;
        }

        case AST_LANZAR: {
            compilar_nodo(chunk, n->datos.lanzar.expresion);
            escribir_chunk(chunk, OP_LANZAR);
            break;
        }

        case AST_IMPORTAR: {
            Result path_v = gc_new_string(n->datos.importar.path);
            int path_idx = agregar_constante(chunk, path_v);
            
            int alias_idx = 0xff;
            if (n->datos.importar.alias) {
                Result alias_v = gc_new_string(n->datos.importar.alias);
                alias_idx = agregar_constante(chunk, alias_v);
            }
            
            escribir_chunk(chunk, OP_IMPORTAR);
            escribir_chunk(chunk, (uint8_t)path_idx);
            escribir_chunk(chunk, (uint8_t)alias_idx);
            escribir_chunk(chunk, (uint8_t)n->datos.importar.n_nombres);
            
            for (int i=0; i < n->datos.importar.n_nombres; i++) {
                Result name_v = gc_new_string(n->datos.importar.nombres[i]);
                int name_idx = agregar_constante(chunk, name_v);
                escribir_chunk(chunk, (uint8_t)name_idx);
            }
            break;
        }

        case AST_LLAMADA: {
            for (int i=0; i < n->datos.llamada.n_args; i++) compilar_nodo(chunk, n->datos.llamada.args[i]);
            
            if (n->es_estatico) {
                Result nombre = gc_new_string(n->datos.llamada.nombre);
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_SUPER);
                escribir_chunk(chunk, (uint8_t)idx);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else if (!strcmp(n->datos.llamada.nombre, "print")) {
                escribir_chunk(chunk, OP_PRINT);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else if (es_funcion_builtin(n->datos.llamada.nombre)) {
                Result nombre = gc_new_string(n->datos.llamada.nombre);
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_OBTENER_GLOBAL);
                escribir_chunk(chunk, (uint8_t)idx);
                
                escribir_chunk(chunk, OP_LLAMAR);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else {
                Result nombre = gc_new_string(n->datos.llamada.nombre);
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_OBTENER_GLOBAL);
                escribir_chunk(chunk, (uint8_t)idx);
                escribir_chunk(chunk, OP_LLAMAR);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            }
            break;
        }

        case AST_FUNCION: {
            if (n->es_publico) escribir_chunk(chunk, OP_MARCAR_EXPORT);
            int f_idx = n_f++;
            funcs[f_idx].jit_ptr = NULL;
            funcs[f_idx].jit_size = 0;
            funcs[f_idx].jit_calls = 0;
            funcs[f_idx].jit_state = 0;
            strcpy(funcs[f_idx].nombre, n->datos.funcion.nombre);
            funcs[f_idx].n_params = n->datos.funcion.n_params;
            for(int i=0; i<n->datos.funcion.n_params; i++) {
                strcpy(funcs[f_idx].params[i], n->datos.funcion.params[i]);
                funcs[f_idx].param_tipos[i] = n->datos.funcion.param_tipos[i];
            }
            funcs[f_idx].tipo_retorno = n->datos.funcion.tipo_retorno;
            funcs[f_idx].es_publico = n->es_publico;
            funcs[f_idx].cuerpo_ast = n->datos.funcion.cuerpo;
            funcs[f_idx].chunk_bytecode = malloc(sizeof(Chunk));
            init_chunk(funcs[f_idx].chunk_bytecode);
            compilar_nodo(funcs[f_idx].chunk_bytecode, n->datos.funcion.cuerpo);
            escribir_chunk(funcs[f_idx].chunk_bytecode, OP_RETORNAR);
            if (ENABLE_OPTIMIZER) optimizar_chunk(funcs[f_idx].chunk_bytecode);
            preparar_cache_chunk(funcs[f_idx].chunk_bytecode);
            
            n->datos.funcion.cuerpo = NULL; 
            hash_insert(&ht_funcs, funcs[f_idx].nombre, f_idx);

            if (n->datos.funcion.n_decoradores > 0) {
                Result nombre_f_res = gc_new_string(n->datos.funcion.nombre);
                int idx_f_const = agregar_constante(chunk, nombre_f_res);

                for (int i = 0; i < n->datos.funcion.n_decoradores; i++) {
                    escribir_chunk(chunk, OP_OBTENER_GLOBAL);
                    escribir_chunk(chunk, (uint8_t)idx_f_const);
                    
                    compilar_nodo(chunk, n->datos.funcion.decoradores[i]);
                    
                    escribir_chunk(chunk, OP_LLAMAR);
                    escribir_chunk(chunk, 1);
                    
                    escribir_chunk(chunk, OP_DEFINIR_GLOBAL);
                    escribir_chunk(chunk, (uint8_t)idx_f_const);
                }
            }
            break;
        }

        case AST_ENUM: {
            if (n->es_publico) escribir_chunk(chunk, OP_MARCAR_EXPORT);
            Result name = gc_new_string(n->datos.enumm.nombre);
            int idx = agregar_constante(chunk, name);
            escribir_chunk(chunk, OP_ENUM);
            escribir_chunk(chunk, (uint8_t)idx);
            escribir_chunk(chunk, (uint8_t)n->datos.enumm.n_variantes);
            
            for (int i = 0; i < n->datos.enumm.n_variantes; i++) {
                Result v_name = gc_new_string(n->datos.enumm.variantes[i]);
                int v_idx = agregar_constante(chunk, v_name);
                escribir_chunk(chunk, (uint8_t)v_idx);
                escribir_chunk(chunk, (uint8_t)n->datos.enumm.n_campos_variante[i]);
            }
            break;
        }

        case AST_LAMBDA: {
            int f_idx = n_f++;
            funcs[f_idx].jit_ptr = NULL;
            funcs[f_idx].jit_size = 0;
            funcs[f_idx].jit_calls = 0;
            funcs[f_idx].jit_state = 0;
            sprintf(funcs[f_idx].nombre, "lambda_%d", ln);
            funcs[f_idx].n_params = n->datos.funcion.n_params;
            for(int i=0; i<n->datos.funcion.n_params; i++) {
                strcpy(funcs[f_idx].params[i], n->datos.funcion.params[i]);
            }
            funcs[f_idx].cuerpo_ast = n->datos.funcion.cuerpo;
            funcs[f_idx].chunk_bytecode = malloc(sizeof(Chunk));
            init_chunk(funcs[f_idx].chunk_bytecode);
            compilar_nodo(funcs[f_idx].chunk_bytecode, n->datos.funcion.cuerpo);
            escribir_chunk(funcs[f_idx].chunk_bytecode, OP_RETORNAR);
            if (ENABLE_OPTIMIZER) optimizar_chunk(funcs[f_idx].chunk_bytecode);
            preparar_cache_chunk(funcs[f_idx].chunk_bytecode);
            
            escribir_chunk(chunk, OP_CLOSURE);
            escribir_chunk(chunk, (uint8_t)f_idx);
            escribir_chunk(chunk, 0);
            break;
        }

        case AST_RETORNAR: {
            if (n->datos.unop.operando && n->datos.unop.operando->tipo == AST_LLAMADA) {
                NodoAST* call = n->datos.unop.operando;
                for (int i = 0; i < call->datos.llamada.n_args; i++) {
                    compilar_nodo(chunk, call->datos.llamada.args[i]);
                }
                Result nombre = gc_new_string(call->datos.llamada.nombre);
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_OBTENER_GLOBAL);
                escribir_chunk(chunk, (uint8_t)idx);
                
                escribir_chunk(chunk, OP_COLA_LLAMAR);
                escribir_chunk(chunk, (uint8_t)call->datos.llamada.n_args);
            } else {
                compilar_nodo(chunk, n->datos.unop.operando);
                escribir_chunk(chunk, OP_RETORNAR);
            }
            break;
        }

        case AST_CLASE: {
            if (n->es_publico) escribir_chunk(chunk, OP_MARCAR_EXPORT);
            for (int i = n->datos.clase.n_miembros - 1; i >= 0; i--) {
                NodoAST* m = n->datos.clase.miembros[i];
                if (m->tipo == AST_ASIGNACION) {
                    if (m->datos.asignacion.valor) compilar_nodo(chunk, m->datos.asignacion.valor);
                    else escribir_chunk(chunk, OP_NULO);
                }
            }

            Result nombre_val = gc_new_string(n->datos.clase.nombre);
            int idx_nombre = agregar_constante(chunk, nombre_val);
            
            int idx_padre = 0;
            if (n->datos.clase.padre) {
                Result padre_val = gc_new_string(n->datos.clase.padre);
                idx_padre = agregar_constante(chunk, padre_val) + 1;
            }
            
            escribir_chunk(chunk, OP_CLASE);
            escribir_chunk(chunk, (uint8_t)idx_nombre);
            escribir_chunk(chunk, (uint8_t)idx_padre);
            escribir_chunk(chunk, (uint8_t)n->datos.clase.n_miembros);
            
            for (int i=0; i < n->datos.clase.n_miembros; i++) {
                NodoAST* m = n->datos.clase.miembros[i];
                Result m_nombre = {0};
                if (m->tipo == AST_FUNCION) m_nombre = gc_new_string(m->datos.funcion.nombre);
                else if (m->tipo == AST_ASIGNACION) m_nombre = gc_new_string(m->datos.asignacion.nombre);
                
                int idx_m_name = agregar_constante(chunk, m_nombre);
                escribir_chunk(chunk, (uint8_t)idx_m_name);
                
                uint8_t flags = 0;
                if (m->es_estatico) flags |= 0x01;
                if (m->es_privado) flags |= 0x02;
                if (m->tipo == AST_FUNCION) flags |= 0x04;
                escribir_chunk(chunk, flags);
                
                if (m->tipo == AST_FUNCION) {
                    compilar_nodo(chunk, m);
                }
            }
            break;
        }

        case AST_NUEVA: {
            for (int i=0; i < n->datos.llamada.n_args; i++) {
                compilar_nodo(chunk, n->datos.llamada.args[i]);
            }
            Result nombre = gc_new_string(n->datos.llamada.nombre);
            int idx = agregar_constante(chunk, nombre);
            escribir_chunk(chunk, OP_NUEVA);
            escribir_chunk(chunk, (uint8_t)idx);
            escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            break;
        }

        case AST_INDICE: {
            compilar_nodo(chunk, n->datos.binop.izquierda);
            compilar_nodo(chunk, n->datos.binop.derecha);
            escribir_chunk(chunk, OP_OBTENER_PROPIEDAD);
            break;
        }

        case AST_ACCESO: {
            compilar_nodo(chunk, n->datos.binop.izquierda);
            compilar_nodo(chunk, n->datos.binop.derecha);
            escribir_chunk(chunk, OP_OBTENER_PROPIEDAD);
            break;
        }

        case AST_METODO: {
            for (int i=0; i < n->datos.llamada.n_args; i++) compilar_nodo(chunk, n->datos.llamada.args[i]);
            compilar_nodo(chunk, n->datos.llamada.receptor);
            
            Result nombre = gc_new_string(n->datos.llamada.nombre);
            int idx = agregar_constante(chunk, nombre);
            escribir_chunk(chunk, OP_INVOQUE_METODO);
            escribir_chunk(chunk, (uint8_t)idx);
            escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            break;
        }

        case AST_ARRAY: {
            escribir_chunk(chunk, OP_ARRAY_CREAR);
            for (int i = 0; i < n->datos.array.count; i++) {
                compilar_nodo(chunk, n->datos.array.elementos[i]);
                escribir_chunk(chunk, OP_ARRAY_APPEND);
            }
            break;
        }

        case AST_COMPREHENSION: {
            escribir_chunk(chunk, OP_ARRAY_CREAR);

            compilar_nodo(chunk, n->datos.comprehension.iterable);

            escribir_chunk(chunk, OP_ITER_INIT);

            int inicio_bucle = chunk->contador;

            escribir_chunk(chunk, OP_ITER_NEXT);

            int salto_fin = chunk->contador;
            escribir_chunk(chunk, OP_SALTAR_SI_FALSO);
            escribir_chunk(chunk, 0xff);
            escribir_chunk(chunk, 0xff);

            Result var_res = gc_new_string(n->datos.comprehension.variables[0]);
            int idx_var = agregar_constante(chunk, var_res);
            escribir_chunk(chunk, OP_DEFINIR_GLOBAL);
            escribir_chunk(chunk, (uint8_t)idx_var);

            if (n->datos.comprehension.condicion) {
                compilar_nodo(chunk, n->datos.comprehension.condicion);
                int salto_skipped = chunk->contador;
                escribir_chunk(chunk, OP_SALTAR_SI_FALSO);
                escribir_chunk(chunk, 0xff);
                escribir_chunk(chunk, 0xff);

                compilar_nodo(chunk, n->datos.comprehension.expresion);
                escribir_chunk(chunk, OP_ARRAY_APPEND);

                int destino_skipped = chunk->contador;
                chunk->codigo[salto_skipped + 1] = (destino_skipped >> 8) & 0xff;
                chunk->codigo[salto_skipped + 2] = destino_skipped & 0xff;
            } else {
                compilar_nodo(chunk, n->datos.comprehension.expresion);
                escribir_chunk(chunk, OP_ARRAY_APPEND);
            }

            escribir_chunk(chunk, OP_SALTAR);
            escribir_chunk(chunk, (inicio_bucle >> 8) & 0xff);
            escribir_chunk(chunk, (uint8_t)(inicio_bucle & 0xff));

            int destino_fin = chunk->contador;
            chunk->codigo[salto_fin + 1] = (destino_fin >> 8) & 0xff;
            chunk->codigo[salto_fin + 2] = destino_fin & 0xff;
            break;
        }

        case AST_C_INCLUIR:
        case AST_C_EXTERN:
            break;

        default:
            break;
    }
}

Chunk* compilar_a_bytecode(NodoAST* nodo) {
    Chunk* chunk = malloc(sizeof(Chunk));
    init_chunk(chunk);
    Chunk* prev_chunk = vm.chunk;
    vm.chunk = chunk; /* GC root during compilation */
    compilar_nodo(chunk, nodo);
    escribir_chunk(chunk, OP_RETORNAR);
    if (ENABLE_OPTIMIZER) optimizar_chunk(chunk);
    preparar_cache_chunk(chunk);
    vm.chunk = prev_chunk;
    return chunk;
}
