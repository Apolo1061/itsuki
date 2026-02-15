#include "linter.h"
#include "itsuki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LinterScope {
    struct LinterScope* padre;
    char nombres[128][MAX_ID_LEN];
    bool usado[128];
    int n_vars;
} LinterScope;

static LinterScope* scope_actual = NULL;

static void push_scope() {
    LinterScope* n = calloc(1, sizeof(LinterScope));
    n->padre = scope_actual;
    scope_actual = n;
}

static void pop_scope() {
    if (!scope_actual) return;
    for (int i = 0; i < scope_actual->n_vars; i++) {
        if (!scope_actual->usado[i]) {
            printf("ADVERTENCIA: Variable '%s' declarada pero no usada.\n", scope_actual->nombres[i]);
        }
    }
    LinterScope* p = scope_actual->padre;
    free(scope_actual);
    scope_actual = p;
}

static void registrar_var(const char* nombre) {
    if (!scope_actual) return;
    for (int i = 0; i < scope_actual->n_vars; i++) {
        if (!strcmp(scope_actual->nombres[i], nombre)) return;
    }
    if (scope_actual->n_vars < 128) {
        strcpy(scope_actual->nombres[scope_actual->n_vars], nombre);
        scope_actual->usado[scope_actual->n_vars] = false;
        scope_actual->n_vars++;
    }
}

static void marcar_usada(const char* nombre) {
    LinterScope* s = scope_actual;
    while (s) {
        for (int i = 0; i < s->n_vars; i++) {
            if (!strcmp(s->nombres[i], nombre)) {
                s->usado[i] = true;
                return;
            }
        }
        s = s->padre;
    }
}

static void lint_nodo(NodoAST* n) {
    if (!n) return;

    switch (n->tipo) {
        case AST_IDENTIFICADOR:
            marcar_usada(n->datos.identificador);
            break;

        case AST_ASIGNACION:
            registrar_var(n->datos.asignacion.nombre);
            lint_nodo(n->datos.asignacion.valor);
            if (n->datos.asignacion.receptor) lint_nodo(n->datos.asignacion.receptor);
            break;

        case AST_BINOP:
            lint_nodo(n->datos.binop.izquierda);
            lint_nodo(n->datos.binop.derecha);
            break;

        case AST_UNOP:
            lint_nodo(n->datos.unop.operando);
            break;

        case AST_LLAMADA:
            if (n->datos.llamada.receptor) lint_nodo(n->datos.llamada.receptor);
            for (int i = 0; i < n->datos.llamada.n_args; i++) {
                lint_nodo(n->datos.llamada.args[i]);
            }
            break;

        case AST_SI:
            lint_nodo(n->datos.si.condicion);
            push_scope(); lint_nodo(n->datos.si.bloque_si); pop_scope();
            push_scope(); lint_nodo(n->datos.si.bloque_sino); pop_scope();
            for (int i = 0; i < n->datos.si.n_elifs; i++) {
                lint_nodo(n->datos.si.elif_conds[i]);
                push_scope(); lint_nodo(n->datos.si.elif_bloques[i]); pop_scope();
            }
            break;

        case AST_MIENTRAS:
            lint_nodo(n->datos.mientras.condicion);
            push_scope(); lint_nodo(n->datos.mientras.cuerpo); pop_scope();
            break;

        case AST_PARA:
            push_scope();
            registrar_var(n->datos.para.var_nombre);
            lint_nodo(n->datos.para.inicio);
            lint_nodo(n->datos.para.fin);
            if (n->datos.para.paso) lint_nodo(n->datos.para.paso);
            lint_nodo(n->datos.para.cuerpo);
            pop_scope();
            break;

        case AST_BLOQUE:
            {
                bool terminador_visto = false;
                for (int i = 0; i < n->datos.bloque.count; i++) {
                    if (terminador_visto) {
                        printf("ADVERTENCIA: Codigo inalcanzable detectado en linea %d.\n", n->datos.bloque.sentencias[i]->linea);
                    }
                    lint_nodo(n->datos.bloque.sentencias[i]);
                    
                    TipoNodoAST t = n->datos.bloque.sentencias[i]->tipo;
                    if (t == AST_RETORNAR) terminador_visto = true;
                }
            }
            break;

        case AST_FUNCION:
            push_scope();
            for (int i = 0; i < n->datos.funcion.n_params; i++) {
                registrar_var(n->datos.funcion.params[i]);
            }
            lint_nodo(n->datos.funcion.cuerpo);
            pop_scope();
            break;

        case AST_RETORNAR:
            lint_nodo(n->datos.unop.operando);
            break;

        case AST_ARRAY:
            for (int i = 0; i < n->datos.array.count; i++) lint_nodo(n->datos.array.elementos[i]);
            break;

        case AST_MAPA:
            for (int i = 0; i < n->datos.mapa.count; i++) {
                lint_nodo(n->datos.mapa.llaves[i]);
                lint_nodo(n->datos.mapa.valores[i]);
            }
            break;

        case AST_COMPREHENSION:
            push_scope();
            for(int i=0; i<n->datos.comprehension.n_variables; i++) registrar_var(n->datos.comprehension.variables[i]);
            lint_nodo(n->datos.comprehension.iterable);
            lint_nodo(n->datos.comprehension.expresion);
            if (n->datos.comprehension.condicion) lint_nodo(n->datos.comprehension.condicion);
            pop_scope();
            break;

        default: break;
    }
}

void itsuki_lint(const char* source) {
    TokenStream* ts_local = tokenize_all(source);
    if (!ts_local || ts_local->count == 0) return;

    TokenStream* ts_old = ts;
    ts = ts_local;
    adv();

    push_scope();
    while (ts->current < ts->count && tk.tipo != TOKEN_EOF) {
        NodoAST* n = parse_stmt();
        if (n) {
            lint_nodo(n);
        }
    }
    pop_scope();

    ts = ts_old;
    free_token_stream(ts_local);
}
