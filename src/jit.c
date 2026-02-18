#include "itsuki.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

typedef double (*JitFn)(Result* args);

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
} JitBuf;

static void emit_u8(JitBuf* b, uint8_t v) {
    if (b->len + 1 > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 128;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len++] = v;
}

static void emit_u32(JitBuf* b, uint32_t v) {
    emit_u8(b, (uint8_t)(v & 0xff));
    emit_u8(b, (uint8_t)((v >> 8) & 0xff));
    emit_u8(b, (uint8_t)((v >> 16) & 0xff));
    emit_u8(b, (uint8_t)((v >> 24) & 0xff));
}

static void emit_u64(JitBuf* b, uint64_t v) {
    emit_u32(b, (uint32_t)(v & 0xffffffffu));
    emit_u32(b, (uint32_t)((v >> 32) & 0xffffffffu));
}

static bool jit_arch_ok() {
#if defined(_M_X64) || defined(__x86_64__)
    return true;
#else
    return false;
#endif
}

static void* jit_alloc_exec(size_t size) {
#if defined(_WIN32)
    void* mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) return NULL;
    return mem;
#else
    void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mem == MAP_FAILED) return NULL;
    return mem;
#endif
}

static void jit_make_exec(void* mem, size_t size) {
#if defined(_WIN32)
    DWORD old;
    VirtualProtect(mem, size, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), mem, size);
#else
    mprotect(mem, size, PROT_READ | PROT_EXEC);
#endif
}

static int param_index(Funcion* f, const char* name) {
    for (int i = 0; i < f->n_params; i++) {
        if (!strcmp(f->params[i], name)) return i;
    }
    return -1;
}

static bool jit_validar_funcion(Funcion* f, int* max_stack) {
    if (!f || !f->chunk_bytecode || !f->chunk_bytecode->codigo) return false;
    Chunk* ch = f->chunk_bytecode;
    int depth = 0;
    int maxd = 0;
    bool has_ret = false;
    int pc = 0;
    while (pc < ch->contador) {
        uint8_t op = ch->codigo[pc];
        switch (op) {
            case OP_CONSTANTE: {
                if (pc + 1 >= ch->contador) return false;
                int idx = ch->codigo[pc + 1];
                if (idx < 0 || idx >= ch->contador_constantes) return false;
                if (ch->constantes[idx].tipo != TIPO_NUMERO) return false;
                depth++;
                if (depth > maxd) maxd = depth;
                pc += 2;
                break;
            }
            case OP_OBTENER_GLOBAL: {
                if (pc + 1 >= ch->contador) return false;
                int idx = ch->codigo[pc + 1];
                if (idx < 0 || idx >= ch->contador_constantes) return false;
                const char* name = ch->constantes[idx].s;
                if (param_index(f, name) < 0) return false;
                depth++;
                if (depth > maxd) maxd = depth;
                pc += 2;
                break;
            }
            case OP_VERDADERO:
            case OP_FALSO:
            case OP_NULO:
                depth++;
                if (depth > maxd) maxd = depth;
                pc += 1;
                break;
            case OP_SUMA:
            case OP_RESTA:
            case OP_MULT:
            case OP_DIV:
            case OP_IGUAL:
            case OP_MAYOR:
            case OP_MENOR:
                if (depth < 2) return false;
                depth--;
                pc += 1;
                break;
            case OP_CONST_SUMA:
            case OP_CONST_RESTA:
            case OP_CONST_MULT:
            case OP_CONST_DIV: {
                if (pc + 1 >= ch->contador) return false;
                int idx = ch->codigo[pc + 1];
                if (idx < 0 || idx >= ch->contador_constantes) return false;
                if (ch->constantes[idx].tipo != TIPO_NUMERO) return false;
                if (depth < 1) return false;
                pc += 2;
                break;
            }
            case OP_NO:
                if (depth < 1) return false;
                pc += 1;
                break;
            case OP_RETORNAR:
                has_ret = true;
                pc += 1;
                while (pc < ch->contador) {
                    if (ch->codigo[pc] != OP_NOP) return false;
                    pc++;
                }
                break;
            case OP_NOP:
                pc += 1;
                break;
            default:
                return false;
        }
    }
    if (!has_ret) return false;
    if (max_stack) *max_stack = maxd;
    return true;
}

static void emit_prologue(JitBuf* b, int stack_bytes, bool win64) {
    emit_u8(b, 0x55);
    emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xE5);
    emit_u8(b, 0x53);
    emit_u8(b, 0x41); emit_u8(b, 0x54);
    emit_u8(b, 0x41); emit_u8(b, 0x55);
    emit_u8(b, 0x48); emit_u8(b, 0x81); emit_u8(b, 0xEC); emit_u32(b, (uint32_t)stack_bytes);
    emit_u8(b, 0x49); emit_u8(b, 0x89); emit_u8(b, 0xE4);
    emit_u8(b, 0x45); emit_u8(b, 0x31); emit_u8(b, 0xED);
    if (win64) { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xCB); }
    else { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xFB); }
}

static void emit_epilogue(JitBuf* b, int stack_bytes) {
    emit_u8(b, 0x48); emit_u8(b, 0x81); emit_u8(b, 0xC4); emit_u32(b, (uint32_t)stack_bytes);
    emit_u8(b, 0x41); emit_u8(b, 0x5D);
    emit_u8(b, 0x41); emit_u8(b, 0x5C);
    emit_u8(b, 0x5B);
    emit_u8(b, 0x5D);
    emit_u8(b, 0xC3);
}

static void emit_inc_r13(JitBuf* b) { emit_u8(b, 0x49); emit_u8(b, 0xFF); emit_u8(b, 0xC5); }
static void emit_dec_r13(JitBuf* b) { emit_u8(b, 0x49); emit_u8(b, 0xFF); emit_u8(b, 0xCD); }

static void emit_movsd_xmm0_m_r12_r13(JitBuf* b) {
    emit_u8(b, 0xF2); emit_u8(b, 0x43); emit_u8(b, 0x0F); emit_u8(b, 0x10); emit_u8(b, 0x04); emit_u8(b, 0xEC);
}

static void emit_movsd_xmm1_m_r12_r13(JitBuf* b) {
    emit_u8(b, 0xF2); emit_u8(b, 0x43); emit_u8(b, 0x0F); emit_u8(b, 0x10); emit_u8(b, 0x0C); emit_u8(b, 0xEC);
}

static void emit_movsd_m_r12_r13_xmm0(JitBuf* b) {
    emit_u8(b, 0xF2); emit_u8(b, 0x43); emit_u8(b, 0x0F); emit_u8(b, 0x11); emit_u8(b, 0x04); emit_u8(b, 0xEC);
}

static void emit_xorpd_xmm0_xmm0(JitBuf* b) { emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, 0x57); emit_u8(b, 0xC0); }
static void emit_xorpd_xmm1_xmm1(JitBuf* b) { emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, 0x57); emit_u8(b, 0xC9); }

static void emit_push_const(JitBuf* b, double val) {
    if (val == 0.0) {
        emit_xorpd_xmm0_xmm0(b);
    } else {
        union { double d; uint64_t u; } v;
        v.d = val;
        emit_u8(b, 0x48); emit_u8(b, 0xB8); emit_u64(b, v.u);
        emit_u8(b, 0x66); emit_u8(b, 0x48); emit_u8(b, 0x0F); emit_u8(b, 0x6E); emit_u8(b, 0xC0);
    }
    emit_movsd_m_r12_r13_xmm0(b);
    emit_inc_r13(b);
}

static void emit_push_arg(JitBuf* b, int arg_index, int res_size) {
    int offset = arg_index * res_size;
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x10); emit_u8(b, 0x83); emit_u32(b, (uint32_t)offset);
    emit_movsd_m_r12_r13_xmm0(b);
    emit_inc_r13(b);
}

static void emit_binop_arith(JitBuf* b, uint8_t opcode) {
    emit_dec_r13(b);
    emit_movsd_xmm1_m_r12_r13(b);
    emit_dec_r13(b);
    emit_movsd_xmm0_m_r12_r13(b);
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, opcode); emit_u8(b, 0xC1);
    emit_movsd_m_r12_r13_xmm0(b);
    emit_inc_r13(b);
}

static void emit_binop_cmp(JitBuf* b, uint8_t setcc) {
    emit_dec_r13(b);
    emit_movsd_xmm1_m_r12_r13(b);
    emit_dec_r13(b);
    emit_movsd_xmm0_m_r12_r13(b);
    emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, 0x2E); emit_u8(b, 0xC1);
    emit_u8(b, 0x0F); emit_u8(b, setcc); emit_u8(b, 0xC0);
    emit_u8(b, 0x0F); emit_u8(b, 0xB6); emit_u8(b, 0xC0);
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x2A); emit_u8(b, 0xC0);
    emit_movsd_m_r12_r13_xmm0(b);
    emit_inc_r13(b);
}

static void emit_unary_no(JitBuf* b) {
    emit_dec_r13(b);
    emit_movsd_xmm0_m_r12_r13(b);
    emit_xorpd_xmm1_xmm1(b);
    emit_u8(b, 0x66); emit_u8(b, 0x0F); emit_u8(b, 0x2E); emit_u8(b, 0xC1);
    emit_u8(b, 0x0F); emit_u8(b, 0x94); emit_u8(b, 0xC0);
    emit_u8(b, 0x0F); emit_u8(b, 0xB6); emit_u8(b, 0xC0);
    emit_u8(b, 0xF2); emit_u8(b, 0x0F); emit_u8(b, 0x2A); emit_u8(b, 0xC0);
    emit_movsd_m_r12_r13_xmm0(b);
    emit_inc_r13(b);
}

static void emit_return(JitBuf* b, int depth) {
    if (depth > 0) {
        emit_dec_r13(b);
        emit_movsd_xmm0_m_r12_r13(b);
    } else {
        emit_xorpd_xmm0_xmm0(b);
    }
}

bool jit_compilar_funcion(int f_idx) {
    if (f_idx < 0 || f_idx >= n_f) return false;
    Funcion* f = &funcs[f_idx];
    if (!jit_arch_ok()) { f->jit_state = -1; return false; }
    if (!f->chunk_bytecode) { f->jit_state = -1; return false; }
    if (f->jit_state == 1) return true;

    int max_stack = 0;
    if (!jit_validar_funcion(f, &max_stack)) { f->jit_state = -1; return false; }

    int stack_bytes = max_stack * (int)sizeof(double);
    if (stack_bytes < 8) stack_bytes = 8;
    stack_bytes = (stack_bytes + 15) & ~15;

    JitBuf buf = {0};
#ifdef _WIN32
    emit_prologue(&buf, stack_bytes, true);
#else
    emit_prologue(&buf, stack_bytes, false);
#endif

    int depth = 0;
    int pc = 0;
    Chunk* ch = f->chunk_bytecode;
    int res_size = (int)sizeof(Result);
    while (pc < ch->contador) {
        uint8_t op = ch->codigo[pc];
        switch (op) {
            case OP_CONSTANTE: {
                int idx = ch->codigo[pc + 1];
                double v = ch->constantes[idx].n;
                emit_push_const(&buf, v);
                depth++;
                pc += 2;
                break;
            }
            case OP_OBTENER_GLOBAL: {
                int idx = ch->codigo[pc + 1];
                const char* name = ch->constantes[idx].s;
                int pidx = param_index(f, name);
                if (pidx < 0) { f->jit_state = -1; free(buf.data); return false; }
                emit_push_arg(&buf, pidx, res_size);
                depth++;
                pc += 2;
                break;
            }
            case OP_VERDADERO:
                emit_push_const(&buf, 1.0);
                depth++;
                pc += 1;
                break;
            case OP_FALSO:
            case OP_NULO:
                emit_push_const(&buf, 0.0);
                depth++;
                pc += 1;
                break;
            case OP_SUMA:
                emit_binop_arith(&buf, 0x58);
                depth--;
                pc += 1;
                break;
            case OP_RESTA:
                emit_binop_arith(&buf, 0x5C);
                depth--;
                pc += 1;
                break;
            case OP_MULT:
                emit_binop_arith(&buf, 0x59);
                depth--;
                pc += 1;
                break;
            case OP_DIV:
                emit_binop_arith(&buf, 0x5E);
                depth--;
                pc += 1;
                break;
            case OP_CONST_SUMA: {
                int idx = ch->codigo[pc + 1];
                double v = ch->constantes[idx].n;
                emit_push_const(&buf, v);
                emit_binop_arith(&buf, 0x58);
                depth--;
                pc += 2;
                break;
            }
            case OP_CONST_RESTA: {
                int idx = ch->codigo[pc + 1];
                double v = ch->constantes[idx].n;
                emit_push_const(&buf, v);
                emit_binop_arith(&buf, 0x5C);
                depth--;
                pc += 2;
                break;
            }
            case OP_CONST_MULT: {
                int idx = ch->codigo[pc + 1];
                double v = ch->constantes[idx].n;
                emit_push_const(&buf, v);
                emit_binop_arith(&buf, 0x59);
                depth--;
                pc += 2;
                break;
            }
            case OP_CONST_DIV: {
                int idx = ch->codigo[pc + 1];
                double v = ch->constantes[idx].n;
                emit_push_const(&buf, v);
                emit_binop_arith(&buf, 0x5E);
                depth--;
                pc += 2;
                break;
            }
            case OP_IGUAL:
                emit_binop_cmp(&buf, 0x94);
                depth--;
                pc += 1;
                break;
            case OP_MAYOR:
                emit_binop_cmp(&buf, 0x97);
                depth--;
                pc += 1;
                break;
            case OP_MENOR:
                emit_binop_cmp(&buf, 0x92);
                depth--;
                pc += 1;
                break;
            case OP_NO:
                emit_unary_no(&buf);
                pc += 1;
                break;
            case OP_RETORNAR:
                emit_return(&buf, depth);
                pc = ch->contador;
                break;
            case OP_NOP:
                pc += 1;
                break;
            default:
                f->jit_state = -1;
                free(buf.data);
                return false;
        }
    }

    emit_epilogue(&buf, stack_bytes);

    void* mem = jit_alloc_exec(buf.len);
    if (!mem) { free(buf.data); f->jit_state = -1; return false; }
    memcpy(mem, buf.data, buf.len);
    jit_make_exec(mem, buf.len);
    free(buf.data);

    f->jit_ptr = mem;
    f->jit_size = (int)buf.len;
    f->jit_state = 1;
    return true;
}

bool jit_ejecutar_funcion(int f_idx, Result args[], int n_args, Result* out) {
    if (f_idx < 0 || f_idx >= n_f) return false;
    Funcion* f = &funcs[f_idx];
    if (f->jit_state != 1 || !f->jit_ptr) return false;
    if (n_args < f->n_params) return false;

    for (int i = 0; i < f->n_params; i++) {
        if (args[i].tipo != TIPO_NUMERO && args[i].tipo != TIPO_BOOL) return false;
    }

    JitFn fn = (JitFn)f->jit_ptr;
    double r = fn(args);
    if (out) {
        out->tipo = TIPO_NUMERO;
        out->n = r;
        out->obj = NULL;
    }
    return true;
}
