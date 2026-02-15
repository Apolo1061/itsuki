#include "itsuki.h"

TryFrame try_stack[MAX_TRY_DEPTH];
int try_sp = 0;
char error_pub_msg[1024];

Macro macros[MAX_MACROS];
int n_m = 0;
HashTable ht_macros;

Clase clases[MAX_FUNCS];
int n_clases = 0;
HashTable ht_clases;

Result evaluar_ast(NodoAST* n) {
    Result r = {0};
    r.tipo = TIPO_NULO;
    if (!n || returning || debe_romper || debe_continuar) return r;

    
    switch(n->tipo) {
        case AST_NUMERO:
            r.n = n->datos.numero;
            r.tipo = TIPO_NUMERO;
            break;
            
        case AST_CADENA:
            r.s = my_strdup(n->datos.cadena);
            r.tipo = TIPO_CADENA;
            break;
            
        case AST_NULO:
            r.tipo = TIPO_NULO;
            break;
            
        case AST_IDENTIFICADOR: {
            int v_idx = hash_lookup(&ht_vars, n->datos.identificador);
            if(v_idx != -1) {
                r.tipo = vars[v_idx].tipo;
                r.tipo_ex = vars[v_idx].tipo_ex;
                if(r.tipo == TIPO_CADENA) r.s = my_strdup(vars[v_idx].val_str);
                else if(r.tipo == TIPO_ARRAY) r.a = vars[v_idx].val_array;
                else if(r.tipo == TIPO_MAP) r.m = vars[v_idx].val_map;
                else if(r.tipo == TIPO_INSTANCIA) r.inst = vars[v_idx].val_inst;
                else if(r.tipo == TIPO_ENUM) { r.enum_val = vars[v_idx].enum_val; r.enum_def = vars[v_idx].enum_def; }
                else r.n = vars[v_idx].val_num;
            } else {
                int f_idx = hash_lookup(&ht_funcs, n->datos.identificador);
                if(f_idx != -1) {
                    r.tipo = TIPO_FUNCION;
                    r.func_index = f_idx;
                } else lanzar_error(ERROR_NOMBRE, "Identificador '%s' no definido", n->datos.identificador);
            }
            break;
        }

        case AST_BINOP: {
            Result izq = evaluar_ast(n->datos.binop.izquierda);
            Result der = evaluar_ast(n->datos.binop.derecha);
            int op = n->datos.binop.operador;
            
            if(op == TOKEN_MAS) {
                if(izq.tipo == TIPO_CADENA || der.tipo == TIPO_CADENA) {
                    char b1[1024], b2[1024];
                    if(izq.tipo == TIPO_CADENA && izq.s) strcpy(b1, izq.s); else sprintf(b1, "%g", izq.n);
                    if(der.tipo == TIPO_CADENA && der.s) strcpy(b2, der.s); else sprintf(b2, "%g", der.n);
                    char* rs = malloc(strlen(b1)+strlen(b2)+1); strcpy(rs, b1); strcat(rs, b2);
                    r.s = rs; r.tipo = TIPO_CADENA;
                } else { r.n = izq.n + der.n; r.tipo = TIPO_NUMERO; }
            } else if(op == TOKEN_MENOS) { r.n = izq.n - der.n; r.tipo = TIPO_NUMERO; }
            else if(op == TOKEN_MULT) { r.n = izq.n * der.n; r.tipo = TIPO_NUMERO; }
            else if(op == TOKEN_DIV) { if(der.n == 0) lanzar_error(ERROR_DIVISION_CERO, "División por cero"); r.n = izq.n / der.n; r.tipo = TIPO_NUMERO; }
            else if(op >= TOKEN_MENOR && op <= TOKEN_DIFERENTE) {
                r.tipo = TIPO_NUMERO;
                switch(op) {
                    case TOKEN_MENOR: r.n = (izq.n < der.n); break;
                    case TOKEN_MAYOR: r.n = (izq.n > der.n); break;
                    case TOKEN_MENOR_IGUAL: r.n = (izq.n <= der.n); break;
                    case TOKEN_MAYOR_IGUAL: r.n = (izq.n >= der.n); break;
                    case TOKEN_IGUAL_IGUAL:
                        if(izq.tipo == TIPO_CADENA && der.tipo == TIPO_CADENA) r.n = (strcmp(izq.s, der.s) == 0);
                        else r.n = (izq.n == der.n);
                        break;
                    case TOKEN_DIFERENTE:
                        if(izq.tipo == TIPO_CADENA && der.tipo == TIPO_CADENA) r.n = (strcmp(izq.s, der.s) != 0);
                        else r.n = (izq.n != der.n);
                        break;
                }
            } else if(op == TOKEN_ES) {
                r.tipo = TIPO_NUMERO;
                if (der.tipo == TIPO_CLASE && izq.tipo == TIPO_INSTANCIA) {
                    Clase* target = &clases[der.clase_index];
                    Clase* curr = izq.inst->clase;
                    r.n = 0;
                    while (curr) { if (curr == target) { r.n = 1; break; } curr = curr->clase_padre; }
                } else if (der.tipo == TIPO_ENUM && izq.tipo == TIPO_ENUM) {
                    if (der.enum_val && izq.enum_val) r.n = (izq.enum_val->definicion == der.enum_val->definicion && izq.enum_val->variante_index == der.enum_val->variante_index);
                    else if (der.enum_def && izq.enum_val) r.n = (izq.enum_val->definicion == der.enum_def);
                } else r.n = (izq.tipo == der.tipo);
            } else if(op == TOKEN_Y) { r.n = (izq.n != 0 && der.n != 0); r.tipo = TIPO_NUMERO; }
            else if(op == TOKEN_O) { r.n = (izq.n != 0 || der.n != 0); r.tipo = TIPO_NUMERO; }
            break;
        }

        case AST_UNOP: {
            Result res = evaluar_ast(n->datos.unop.operando);
            if(n->datos.unop.operador == TOKEN_MENOS) { r.n = -res.n; r.tipo = TIPO_NUMERO; }
            else if(n->datos.unop.operador == TOKEN_NO) { r.n = (res.n == 0); r.tipo = TIPO_NUMERO; }
            break;
        }

        case AST_ASIGNACION: {
            Result val = evaluar_ast(n->datos.asignacion.valor);
            aplicar_limites_tipo(&val, n->tipo_ex);
            int v_idx = hash_lookup(&ht_vars, n->datos.asignacion.nombre);
            if(v_idx == -1) {
                v_idx = n_v++;
                strcpy(vars[v_idx].nombre, n->datos.asignacion.nombre);
                hash_insert(&ht_vars, vars[v_idx].nombre, v_idx);
            }
            liberar_variable(&vars[v_idx]);
            vars[v_idx].tipo = val.tipo;
            vars[v_idx].tipo_ex = n->tipo_ex;
            if(val.tipo == TIPO_CADENA) vars[v_idx].val_str = val.s;
            else if(val.tipo == TIPO_ARRAY) vars[v_idx].val_array = val.a;
            else if(val.tipo == TIPO_MAP) vars[v_idx].val_map = val.m;
            else if(val.tipo == TIPO_INSTANCIA) vars[v_idx].val_inst = val.inst;
            else vars[v_idx].val_num = val.n;
            r = val;
            break;
        }

        case AST_LLAMADA: {
            if(!strcmp(n->datos.llamada.nombre, "printf")) {
                Result arg = evaluar_ast(n->datos.llamada.args[0]);
                if(arg.tipo == TIPO_CADENA) printf("%s\n", arg.s);
                else printf("%g\n", arg.n);
                fflush(stdout);
                break;
            }
            Result args_ev[16]; int na = n->datos.llamada.n_args;
            for(int i=0; i<na && i<16; i++) args_ev[i] = evaluar_ast(n->datos.llamada.args[i]);
            
            if(es_funcion_builtin(n->datos.llamada.nombre)) {
                r = ejecutar_builtin(n->datos.llamada.nombre, args_ev, na);
            } else {
                int f_idx = hash_lookup(&ht_funcs, n->datos.llamada.nombre);
                if(f_idx != -1) {
                    r = llamar_funcion_usuario(f_idx, args_ev, na);
                } else lanzar_error(ERROR_NOMBRE, "Funcion '%s' no definida", n->datos.llamada.nombre);
            }
            break;
        }

        case AST_BLOQUE: {
            for(int i=0; i<n->datos.bloque.count && !returning && !debe_romper && !debe_continuar; i++) {
                r = evaluar_ast(n->datos.bloque.sentencias[i]);
            }
            break;
        }

        case AST_SI: {
            Result cond = evaluar_ast(n->datos.si.condicion);
            if(cond.n != 0) {
                r = evaluar_ast(n->datos.si.bloque_si);
            } else if(n->datos.si.bloque_sino) {
                r = evaluar_ast(n->datos.si.bloque_sino);
            }
            break;
        }

        case AST_MIENTRAS: {
            while(evaluar_ast(n->datos.mientras.condicion).n != 0 && !returning && !debe_romper) {
                evaluar_ast(n->datos.mientras.cuerpo);
                if(debe_romper) { debe_romper = false; break; }
                if(debe_continuar) debe_continuar = false;
            }
            break;
        }

        case AST_PARA: {
            Result start = evaluar_ast(n->datos.para.inicio);
            Result end = evaluar_ast(n->datos.para.fin);
            double step = n->datos.para.paso ? evaluar_ast(n->datos.para.paso).n : 1.0;
            int v_idx = hash_lookup(&ht_vars, n->datos.para.var_nombre);
            if(v_idx == -1) {
                v_idx = n_v++;
                strcpy(vars[v_idx].nombre, n->datos.para.var_nombre);
                hash_insert(&ht_vars, vars[v_idx].nombre, v_idx);
            }
            for(double i=start.n; i<end.n; i+=step) {
                vars[v_idx].val_num = i; vars[v_idx].tipo = TIPO_NUMERO;
                evaluar_ast(n->datos.para.cuerpo);
                if(debe_romper) { debe_romper = false; break; }
                if(debe_continuar) debe_continuar = false;
                if(returning) break;
            }
            break;
        }

        case AST_FUNCION: {
            int f_idx = n_f++;
            strcpy(funcs[f_idx].nombre, n->datos.funcion.nombre);
            funcs[f_idx].n_params = n->datos.funcion.n_params;
            for(int i=0; i<n->datos.funcion.n_params; i++) {
                strcpy(funcs[f_idx].params[i], n->datos.funcion.params[i]);
            }
            funcs[f_idx].cuerpo_ast = n->datos.funcion.cuerpo;
            n->datos.funcion.cuerpo = NULL; 
            hash_insert(&ht_funcs, funcs[f_idx].nombre, f_idx);
            break;
        }

        case AST_RETORNAR: {
            r = evaluar_ast(n->datos.unop.operando);
            returning = true;
            last_res = r;
            break;
        }

        default: break;
    }
    return r;
}

Result llamar_funcion_usuario(int f_idx, Result args[], int n_args) {
    if(f_idx < 0 || f_idx >= n_f) lanzar_error(ERROR_NOMBRE, "Índice de función inválido");
    
    HashNode* saved_scope;
    hash_enter_scope(&saved_scope);
    int old_nv = n_v;
    
    for(int i = 0; i < funcs[f_idx].n_params && i < n_args; i++) {
        int vi = n_v++;
        strcpy(vars[vi].nombre, funcs[f_idx].params[i]);
        vars[vi].tipo = args[i].tipo;
        if(vars[vi].tipo == TIPO_CADENA) vars[vi].val_str = args[i].s;
        else if(vars[vi].tipo == TIPO_ARRAY) vars[vi].val_array = args[i].a;
        else if(vars[vi].tipo == TIPO_MAP) vars[vi].val_map = args[i].m;
        else if(vars[vi].tipo == TIPO_INSTANCIA) vars[vi].val_inst = args[i].inst;
        else if(vars[vi].tipo == TIPO_ENUM) { vars[vi].enum_val = args[i].enum_val; vars[vi].enum_def = args[i].enum_def; }
        else vars[vi].val_num = args[i].n;
        hash_insert(&ht_vars, vars[vi].nombre, vi);
    }
    
    Result res;
    res.tipo = TIPO_NULO;
    res.obj = NULL;
    if(funcs[f_idx].cuerpo_ast) {
        res = evaluar_ast(funcs[f_idx].cuerpo_ast);
    }
    
    returning = false;
    for(int i = old_nv; i < n_v; i++) liberar_variable(&vars[i]);
    n_v = old_nv;
    hash_exit_scope(&saved_scope);
    
    return res;
}

void itsuki_init() {
    lista_objetos = NULL;
    contador_objetos = 0;
    umbral_gc = 1024;

    srand(time(NULL));
    memset(&ht_vars, 0, sizeof(HashTable));
    memset(&ht_funcs, 0, sizeof(HashTable));
    memset(&ht_macros, 0, sizeof(HashTable));
    memset(&ht_clases, 0, sizeof(HashTable));
    current_scope_nodes = NULL;
    ln = 1; n_v = 0; n_f = 0; n_m = 0; n_clases = 0;

    registrar_builtins();
    vm_init();
}

void itsuki_execute(const char* src) {
    if(!src || !*src) return;
    returning = false; debe_romper = false; debe_continuar = false;
    
    ts = tokenize_all(src);
    if(!ts) return;
    
    adv();
    while (tk.tipo != TOKEN_EOF) {
        NodoAST* node = parse_stmt();
        if(node) {
            Chunk* chunk = compilar_a_bytecode(node);
            vm_ejecutar(chunk);
            free_chunk(chunk);
            free(chunk);
            free_ast(node);
        }
        if (tk.tipo == TOKEN_PUNTO_COMA) adv();
    }
    
    free_token_stream(ts);
    ts = NULL;
}

void main_exec(const char* src) {
    itsuki_init();
    itsuki_execute(src);
}
