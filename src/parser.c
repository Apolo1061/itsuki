#include "itsuki.h"
#include <stdarg.h>

NodoAST* crear_nodo(TipoNodoAST tipo) {
    NodoAST* n = calloc(1, sizeof(NodoAST));
    n->tipo = tipo;
    n->linea = ln;
    n->tipo_ex = TEX_AUTO;
    return n;
}

NodoAST* crear_nodo_numero(double num) {
    NodoAST* n = crear_nodo(AST_NUMERO);
    n->datos.numero = num;
    return n;
}

NodoAST* crear_nodo_cadena(const char* s) {
    NodoAST* n = crear_nodo(AST_CADENA);
    n->datos.cadena = my_strdup(s);
    return n;
}

NodoAST* crear_nodo_identificador(const char* id) {
    NodoAST* n = crear_nodo(AST_IDENTIFICADOR);
    n->datos.identificador = my_strdup(id);
    return n;
}

NodoAST* crear_nodo_binop(NodoAST* izq, int op, NodoAST* der) {
    NodoAST* n = crear_nodo(AST_BINOP);
    n->datos.binop.izquierda = izq;
    n->datos.binop.operador = op;
    n->datos.binop.derecha = der;
    return n;
}

NodoAST* crear_nodo_unop(int op, NodoAST* operando) {
    NodoAST* n = crear_nodo(AST_UNOP);
    n->datos.unop.operador = op;
    n->datos.unop.operando = operando;
    return n;
}

NodoAST* crear_nodo_asignacion(const char* nombre, NodoAST* valor) {
    NodoAST* n = crear_nodo(AST_ASIGNACION);
    n->datos.asignacion.nombre = my_strdup(nombre);
    n->datos.asignacion.valor = valor;
    return n;
}

NodoAST* crear_nodo_llamada(const char* nombre, NodoAST** args, int n_args) {
    NodoAST* n = crear_nodo(AST_LLAMADA);
    n->datos.llamada.nombre = my_strdup(nombre);
    n->datos.llamada.args = args;
    n->datos.llamada.n_args = n_args;
    return n;
}

NodoAST* crear_nodo_si(NodoAST* condicion, NodoAST* bloque_si, NodoAST* bloque_sino, NodoAST** elif_conds, NodoAST** elif_bloques, int n_elifs) {
    NodoAST* n = crear_nodo(AST_SI);
    n->datos.si.condicion = condicion;
    n->datos.si.bloque_si = bloque_si;
    n->datos.si.bloque_sino = bloque_sino;
    n->datos.si.elif_conds = elif_conds;
    n->datos.si.elif_bloques = elif_bloques;
    n->datos.si.n_elifs = n_elifs;
    return n;
}

NodoAST* crear_nodo_mientras(NodoAST* condicion, NodoAST* cuerpo) {
    NodoAST* n = crear_nodo(AST_MIENTRAS);
    n->datos.mientras.condicion = condicion;
    n->datos.mientras.cuerpo = cuerpo;
    return n;
}

NodoAST* crear_nodo_para(const char* var, NodoAST* inicio, NodoAST* fin, NodoAST* paso, NodoAST* cuerpo) {
    NodoAST* n = crear_nodo(AST_PARA);
    n->datos.para.var_nombre = my_strdup(var);
    n->datos.para.inicio = inicio;
    n->datos.para.fin = fin;
    n->datos.para.paso = paso;
    n->datos.para.cuerpo = cuerpo;
    return n;
}

NodoAST* crear_nodo_funcion(const char* nombre, char** params, TipoExacto* param_tipos, int n_params, NodoAST* cuerpo, TipoExacto tipo_retorno) {
    NodoAST* n = crear_nodo(AST_FUNCION);
    n->datos.funcion.nombre = my_strdup(nombre);
    n->datos.funcion.params = params;
    n->datos.funcion.param_tipos = param_tipos;
    n->datos.funcion.n_params = n_params;
    n->datos.funcion.cuerpo = cuerpo;
    n->datos.funcion.tipo_retorno = tipo_retorno;
    return n;
}

NodoAST* crear_nodo_bloque(NodoAST** sentencias, int count) {
    NodoAST* n = crear_nodo(AST_BLOQUE);
    n->datos.bloque.sentencias = sentencias;
    n->datos.bloque.count = count;
    return n;
}

NodoAST* crear_nodo_array(NodoAST** elementos, int count) {
    NodoAST* n = crear_nodo(AST_ARRAY);
    n->datos.array.elementos = elementos;
    n->datos.array.count = count;
    return n;
}

NodoAST* crear_nodo_mapa(NodoAST** llaves, NodoAST** valores, int count) {
    NodoAST* n = crear_nodo(AST_MAPA);
    n->datos.mapa.llaves = llaves;
    n->datos.mapa.valores = valores;
    n->datos.mapa.count = count;
    return n;
}

NodoAST* crear_nodo_clase(const char* nombre, const char* padre, NodoAST** miembros, int n_miembros) {
    NodoAST* n = crear_nodo(AST_CLASE);
    n->datos.clase.nombre = my_strdup(nombre);
    n->datos.clase.padre = padre ? my_strdup(padre) : NULL;
    n->datos.clase.miembros = miembros;
    n->datos.clase.n_miembros = n_miembros;
    return n;
}

NodoAST* crear_nodo_nulo() {
    return crear_nodo(AST_NULO);
}

NodoAST* crear_nodo_enum(const char* nombre, char** variante_nombres, int* variante_n_campos, int n_variantes) {
    NodoAST* n = crear_nodo(AST_ENUM);
    n->datos.enumm.nombre[0] = 0;
    if (nombre) strcpy(n->datos.enumm.nombre, nombre);
    n->datos.enumm.variantes = variante_nombres;
    n->datos.enumm.n_campos_variante = variante_n_campos;
    n->datos.enumm.n_variantes = n_variantes;
    return n;
}

NodoAST* crear_nodo_try_catch(NodoAST* bloque_try, const char* var_error, NodoAST* bloque_catch, NodoAST* bloque_finally) {
    NodoAST* n = crear_nodo(AST_TRY_CATCH);
    n->datos.try_catch.bloque_try = bloque_try;
    n->datos.try_catch.var_error[0] = 0;
    if (var_error) strcpy(n->datos.try_catch.var_error, var_error);
    n->datos.try_catch.bloque_catch = bloque_catch;
    n->datos.try_catch.bloque_finally = bloque_finally;
    return n;
}

NodoAST* crear_nodo_importar(const char* path, const char* alias, char** nombres, int n_nombres) {
    NodoAST* n = crear_nodo(AST_IMPORTAR);
    n->datos.importar.path = path ? my_strdup(path) : NULL;
    n->datos.importar.alias = alias ? my_strdup(alias) : NULL;
    n->datos.importar.nombres = nombres;
    n->datos.importar.n_nombres = n_nombres;
    return n;
}

NodoAST* crear_nodo_lanzar(NodoAST* expresion) {
    NodoAST* n = crear_nodo(AST_LANZAR);
    n->datos.lanzar.expresion = expresion;
    return n;
}

void free_ast(NodoAST* n) {
    if(!n) return;
    switch(n->tipo) {
        case AST_CADENA: free(n->datos.cadena); break;
        case AST_IDENTIFICADOR: free(n->datos.identificador); break;
        case AST_BINOP: free_ast(n->datos.binop.izquierda); free_ast(n->datos.binop.derecha); break;
        case AST_UNOP: free_ast(n->datos.unop.operando); break;
        case AST_ASIGNACION: free(n->datos.asignacion.nombre); free_ast(n->datos.asignacion.valor); break;
        case AST_LLAMADA:
            free(n->datos.llamada.nombre);
            for(int i=0; i<n->datos.llamada.n_args; i++) free_ast(n->datos.llamada.args[i]);
            free(n->datos.llamada.args);
            break;
        case AST_SI:
            free_ast(n->datos.si.condicion);
            free_ast(n->datos.si.bloque_si);
            free_ast(n->datos.si.bloque_sino);
            for(int i=0; i<n->datos.si.n_elifs; i++) {
                free_ast(n->datos.si.elif_conds[i]);
                free_ast(n->datos.si.elif_bloques[i]);
            }
            free(n->datos.si.elif_conds);
            free(n->datos.si.elif_bloques);
            break;
        case AST_MIENTRAS:
            free_ast(n->datos.mientras.condicion);
            free_ast(n->datos.mientras.cuerpo);
            break;
        case AST_PARA:
            free(n->datos.para.var_nombre);
            free_ast(n->datos.para.inicio);
            free_ast(n->datos.para.fin);
            free_ast(n->datos.para.paso);
            free_ast(n->datos.para.cuerpo);
            break;
        case AST_FUNCION:
            free(n->datos.funcion.nombre);
            for(int i=0; i<n->datos.funcion.n_params; i++) free(n->datos.funcion.params[i]);
            free(n->datos.funcion.params);
            free(n->datos.funcion.param_tipos);
            free_ast(n->datos.funcion.cuerpo);
            break;
        case AST_BLOQUE:
            for(int i=0; i<n->datos.bloque.count; i++) free_ast(n->datos.bloque.sentencias[i]);
            free(n->datos.bloque.sentencias);
            break;
        case AST_ARRAY:
            for(int i=0; i<n->datos.array.count; i++) free_ast(n->datos.array.elementos[i]);
            free(n->datos.array.elementos);
            break;
        case AST_MAPA:
            for(int i=0; i<n->datos.mapa.count; i++) {
                free_ast(n->datos.mapa.llaves[i]);
                free_ast(n->datos.mapa.valores[i]);
            }
            free(n->datos.mapa.llaves);
            free(n->datos.mapa.valores);
            break;
          case AST_ENUM:
            for(int i=0; i<n->datos.enumm.n_variantes; i++) free(n->datos.enumm.variantes[i]);
            free(n->datos.enumm.variantes);
            free(n->datos.enumm.n_campos_variante);
            break;
        case AST_TRY_CATCH:
            free_ast(n->datos.try_catch.bloque_try);
            free_ast(n->datos.try_catch.bloque_catch);
            free_ast(n->datos.try_catch.bloque_finally);
            break;
        case AST_LANZAR:
            free_ast(n->datos.lanzar.expresion);
            break;
        case AST_NULO: break;
        default: break;
    }
    free(n);
}

NodoAST* val() {
    if (tk.tipo == TOKEN_NUMERO) {
        NodoAST* n = crear_nodo_numero(atof(tk.valor));
        adv();
        return n;
    }
    else if (tk.tipo == TOKEN_CADENA) {
        NodoAST* n = crear_nodo_cadena(tk.valor);
        adv();
        return n;
    }
    else if (tk.tipo == TOKEN_FSTRING) {
        char* full_content = my_strdup(tk.valor);
        adv();
        NodoAST* root = NULL;
        int i = 0;
        int start = 0;
        while (full_content[i]) {
            if (full_content[i] == '{') {
                if (i > start) {
                    char* fragment = malloc(i - start + 1);
                    strncpy(fragment, full_content + start, i - start);
                    fragment[i - start] = 0;
                    NodoAST* lit = crear_nodo_cadena(fragment);
                    free(fragment);
                    if (!root) root = lit;
                    else root = crear_nodo_binop(root, TOKEN_MAS, lit);
                }
                
                i++;
                int expr_start = i;
                int depth = 1;
                while (full_content[i] && depth > 0) {
                    if (full_content[i] == '{') depth++;
                    else if (full_content[i] == '}') depth--;
                    if (depth > 0) i++;
                }
                
                if (full_content[i] == '}') {
                    char* expr_str = malloc(i - expr_start + 1);
                    strncpy(expr_str, full_content + expr_start, i - expr_start);
                    expr_str[i - expr_start] = 0;
                    
                    TokenStream* old_ts = ts;
                    Token old_tk = tk;
                    
                    ts = tokenize_all(expr_str);
                    ts->current = 0;
                    adv();
                    NodoAST* expr_node = logica();
                    
                    NodoAST** args = malloc(sizeof(NodoAST*));
                    args[0] = expr_node;
                    NodoAST* call_node = crear_nodo_llamada("string", args, 1);
                    
                    if (!root) root = call_node;
                    else root = crear_nodo_binop(root, TOKEN_MAS, call_node);
                    
                    free_token_stream(ts);
                    ts = old_ts;
                    tk = old_tk;
                    free(expr_str);
                    i++;
                    start = i;
                }
            } else {
                i++;
            }
        }
        
        if (i > start) {
            char* fragment = my_strdup(full_content + start);
            NodoAST* lit = crear_nodo_cadena(fragment);
            free(fragment);
            if (!root) root = lit;
            else root = crear_nodo_binop(root, TOKEN_MAS, lit);
        }
        
        free(full_content);
        if (!root) return crear_nodo_cadena("");
        return root;
    }
    else if (tk.tipo == TOKEN_NULO) {
        adv();
        return crear_nodo_nulo();
    }
    else if (tk.tipo == TOKEN_CORCHETE_IZQ) {
        adv();
        
        if (tk.tipo == TOKEN_CORCHETE_DER) {
            adv();
            return crear_nodo_array(NULL, 0);
        }
        
        NodoAST* first_expr = logica();
        
        if (tk.tipo == TOKEN_PARA) {
            adv();
            
            NodoAST* n = crear_nodo(AST_COMPREHENSION);
            n->datos.comprehension.expresion = first_expr;
            n->datos.comprehension.es_dict = false;
            
            n->datos.comprehension.variables = malloc(sizeof(char*) * 2);
            n->datos.comprehension.variables[0] = my_strdup(tk.valor);
            n->datos.comprehension.n_variables = 1;
            adv();
            
            if (tk.tipo == TOKEN_EN) adv();
            
            n->datos.comprehension.iterable = logica();
            
            if (tk.tipo == TOKEN_SI) {
                adv();
                n->datos.comprehension.condicion = logica();
            } else {
                n->datos.comprehension.condicion = NULL;
            }
            
            if (tk.tipo == TOKEN_CORCHETE_DER) adv();
            return n;
        } else {
            NodoAST** elementos = NULL;
            int count = 0;
            int cap = 10;
            elementos = malloc(sizeof(NodoAST*) * cap);
            elementos[count++] = first_expr;
            
            if (tk.tipo == TOKEN_COMA) adv();
            
            while(tk.tipo != TOKEN_CORCHETE_DER && tk.tipo != TOKEN_EOF) {
                if (count >= cap) {
                    cap *= 2;
                    elementos = realloc(elementos, sizeof(NodoAST*) * cap);
                }
                elementos[count++] = logica();
                if(tk.tipo == TOKEN_COMA) adv();
            }
            if(tk.tipo == TOKEN_CORCHETE_DER) adv();
            return crear_nodo_array(elementos, count);
        }
    }
    else if (tk.tipo == TOKEN_LLAVE_IZQ) {
        adv();
        NodoAST** llaves = NULL;
        NodoAST** valores = NULL;
        int count = 0;
        int cap = 10;
        llaves = malloc(sizeof(NodoAST*) * cap);
        valores = malloc(sizeof(NodoAST*) * cap);
        while(tk.tipo != TOKEN_LLAVE_DER && tk.tipo != TOKEN_EOF) {
            if (count >= cap) {
                cap *= 2;
                llaves = realloc(llaves, sizeof(NodoAST*) * cap);
                valores = realloc(valores, sizeof(NodoAST*) * cap);
            }
            llaves[count] = logica();
            if(tk.tipo == TOKEN_DOS_PUNTOS) adv();
            valores[count] = logica();
            count++;
            if(tk.tipo == TOKEN_COMA) adv();
        }
        if(tk.tipo == TOKEN_LLAVE_DER) adv();
        return crear_nodo_mapa(llaves, valores, count);
    }
    else if (tk.tipo == TOKEN_NUEVA) {
        adv();
        char* nombre_clase = my_strdup(tk.valor);
        adv();
        NodoAST** args = NULL;
        int n_args = 0;
        if(tk.tipo == TOKEN_PAR_IZQ) {
            adv();
            while(tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
                args = realloc(args, sizeof(NodoAST*) * (n_args + 1));
                args[n_args++] = logica();
                if(tk.tipo == TOKEN_COMA) adv();
            }
            if(tk.tipo == TOKEN_PAR_DER) adv();
        }
        NodoAST* n = crear_nodo(AST_NUEVA);
        n->datos.llamada.nombre = nombre_clase;
        n->datos.llamada.args = args;
        n->datos.llamada.n_args = n_args;
        return n;
    }
    else if (tk.tipo == TOKEN_SUPER) {
        adv();
        if (tk.tipo == TOKEN_PUNTO) {
            adv();
            char* metodo = my_strdup(tk.valor);
            adv();
            if (tk.tipo == TOKEN_PAR_IZQ) {
                adv();
                NodoAST** args = NULL;
                int n_args = 0;
                while(tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
                    args = realloc(args, sizeof(NodoAST*) * (n_args + 1));
                    args[n_args++] = logica();
                    if(tk.tipo == TOKEN_COMA) adv();
                }
                if(tk.tipo == TOKEN_PAR_DER) adv();
                
                NodoAST* n = crear_nodo(AST_LLAMADA);
                n->datos.llamada.nombre = metodo;
                n->datos.llamada.args = args;
                n->datos.llamada.n_args = n_args;
                n->es_estatico = true;
                return n;
            }
        }
        lanzar_error(ERROR_SINTAXIS, "Se esperaba . después de super");
    }
    else if (tk.tipo == TOKEN_IDENTIFICADOR || tk.tipo == TOKEN_LEER) {
        char name[MAX_ID_LEN]; strcpy(name, tk.valor); adv();
        if (tk.tipo == TOKEN_PAR_IZQ) { 
            adv();
            NodoAST** args = NULL;
            int n_args = 0;
            while(tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
                args = realloc(args, sizeof(NodoAST*) * (n_args + 1));
                args[n_args++] = logica();
                if(tk.tipo == TOKEN_COMA) adv();
            }
            if(tk.tipo == TOKEN_PAR_DER) adv();
            return crear_nodo_llamada(name, args, n_args);
        }
        
        if (tk.tipo == TOKEN_CORCHETE_IZQ) {
            adv();
            NodoAST* indice = logica();
            if(tk.tipo == TOKEN_CORCHETE_DER) adv();
            NodoAST* n = crear_nodo(AST_INDICE);
            n->datos.binop.izquierda = crear_nodo_identificador(name);
            n->datos.binop.derecha = indice;
            return n;
        }
        
        if (tk.tipo == TOKEN_PUNTO) {
            adv();
            char prop[64]; strcpy(prop, tk.valor); adv();
            NodoAST* receptor = crear_nodo_identificador(name);
            
            if (tk.tipo == TOKEN_PAR_IZQ) {
                adv();
                NodoAST** args = NULL;
                int n_args = 0;
                while(tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
                    args = realloc(args, sizeof(NodoAST*) * (n_args + 1));
                    args[n_args++] = logica();
                    if(tk.tipo == TOKEN_COMA) adv();
                }
                if(tk.tipo == TOKEN_PAR_DER) adv();
                
                NodoAST* n = crear_nodo(AST_METODO);
                n->datos.llamada.nombre = my_strdup(prop);
                n->datos.llamada.receptor = receptor;
                n->datos.llamada.args = args;
                n->datos.llamada.n_args = n_args;
                return n;
            }
            NodoAST* n = crear_nodo(AST_ACCESO);
            n->datos.binop.izquierda = receptor;
            n->datos.binop.derecha = crear_nodo_cadena(prop);
            return n;
        }
        
        return crear_nodo_identificador(name);
    } else if (tk.tipo == TOKEN_PAR_IZQ) { 
        adv(); NodoAST* n = logica(); if(tk.tipo==TOKEN_PAR_DER) adv(); return n;
    } else if (tk.tipo == TOKEN_LAMBDA) {
        adv();
        if (tk.tipo != TOKEN_PAR_IZQ) lanzar_error(ERROR_SINTAXIS, "Se esperaba '(' tras lambda");
        adv();
        char** params = malloc(sizeof(char*) * 8);
        TipoExacto* param_tipos = malloc(sizeof(TipoExacto) * 8);
        int n_params = 0;
        while (tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
            if (tk.tipo == TOKEN_IDENTIFICADOR) {
                params[n_params] = my_strdup(tk.valor);
                adv();
                param_tipos[n_params] = TEX_AUTO;
                if (tk.tipo == TOKEN_DOS_PUNTOS) {
                    adv();
                    param_tipos[n_params] = string_a_tipo_exacto(tk.valor);
                    adv();
                }
                n_params++;
            }
            if (tk.tipo == TOKEN_COMA) adv();
        }
        if (tk.tipo == TOKEN_PAR_DER) adv();
        
        if (tk.tipo != TOKEN_LLAVE_IZQ) lanzar_error(ERROR_SINTAXIS, "Se esperaba '{' tras parametros de lambda");
        NodoAST* cuerpo = parse_block();
        
        char anon_name[64]; sprintf(anon_name, "lambda_%d", ln);
        NodoAST* n = crear_nodo_funcion(anon_name, params, param_tipos, n_params, cuerpo, TEX_AUTO);
        n->tipo = AST_LAMBDA;
        return n;
    }
    else {
        lanzar_error(ERROR_SINTAXIS, "Se esperaba un valor, token inesperado: %s", tk.valor);
    }
    return crear_nodo_nulo();
}

NodoAST* comparacion() {
    NodoAST* r = expr();
    while ((tk.tipo >= TOKEN_MENOR && tk.tipo <= TOKEN_DIFERENTE) || tk.tipo == TOKEN_ES) {
        int op = tk.tipo;
        adv();
        r = crear_nodo_binop(r, op, expr());
    }
    return r;
}


NodoAST* logica() {
    NodoAST* r = comparacion();
    while(tk.tipo == TOKEN_Y || tk.tipo == TOKEN_O) {
        int op = tk.tipo;
        adv();
        r = crear_nodo_binop(r, op, comparacion());
    }
    return r;
}

NodoAST* unary() {
    if (tk.tipo == TOKEN_MENOS) {
        adv();
        return crear_nodo_unop(TOKEN_MENOS, unary());
    }
    if (tk.tipo == TOKEN_NO) {
        adv();
        return crear_nodo_unop(TOKEN_NO, unary());
    }
    return val();
}

NodoAST* term() { 
    NodoAST* r = unary(); 
    while(tk.tipo == TOKEN_MULT || tk.tipo == TOKEN_DIV || tk.tipo == TOKEN_MODULO) { 
        int op = tk.tipo; adv(); 
        r = crear_nodo_binop(r, op, unary()); 
    } 
    return r; 
}

NodoAST* expr() { 
    NodoAST* r = term(); 
    while(tk.tipo == TOKEN_MAS || tk.tipo == TOKEN_MENOS) { 
        int op = tk.tipo; adv(); 
        r = crear_nodo_binop(r, op, term()); 
    } 
    return r; 
}
NodoAST* parse_stmt();

NodoAST* parse_block() {
    if (tk.tipo == TOKEN_LLAVE_IZQ) adv();
    NodoAST** sentencias = NULL;
    int count = 0;
    while(tk.tipo != TOKEN_LLAVE_DER && tk.tipo != TOKEN_EOF) {
        sentencias = realloc(sentencias, sizeof(NodoAST*) * (count + 1));
        sentencias[count++] = parse_stmt();
        if (tk.tipo == TOKEN_PUNTO_COMA) adv();
    }
    if (tk.tipo == TOKEN_LLAVE_DER) adv();
    return crear_nodo_bloque(sentencias, count);
}

NodoAST* parse_stmt() {
    bool exportar = false;
    if (tk.tipo == TOKEN_EXPORTAR) {
        exportar = true;
        adv();
        if (tk.tipo != TOKEN_SEA && tk.tipo != TOKEN_FUNCION && tk.tipo != TOKEN_CLASE && tk.tipo != TOKEN_ENUM) {
                 lanzar_error(ERROR_SINTAXIS, "Se esperaba una declaracion (sea, funcion, clase, enum) tras 'exportar'");
        }
    }

    if (tk.tipo == TOKEN_ARROBA) {
        NodoAST** decs = NULL;
        int n_decs = 0;
        while (tk.tipo == TOKEN_ARROBA) {
            adv();
            decs = realloc(decs, sizeof(NodoAST*) * (n_decs + 1));
            decs[n_decs++] = logica();
        }
        if (tk.tipo != TOKEN_FUNCION) lanzar_error(ERROR_SINTAXIS, "Se esperaba 'funcion' tras decorador");
        
        NodoAST* func_node = parse_stmt();
        if (func_node->tipo == AST_FUNCION) {
            func_node->datos.funcion.decoradores = decs;
            func_node->datos.funcion.n_decoradores = n_decs;
        }
        if (exportar) func_node->es_publico = true;
        return func_node;
    }

    if (tk.tipo == TOKEN_IMPORTAR) {
        adv();
        if (tk.tipo != TOKEN_CADENA) lanzar_error(ERROR_SINTAXIS, "Se esperaba una cadena con la ruta del modulo");
        char* path = my_strdup(tk.valor);
        adv();
        char* alias = NULL;
        if (tk.tipo == TOKEN_COMO) {
            adv();
            if (tk.tipo != TOKEN_IDENTIFICADOR) lanzar_error(ERROR_SINTAXIS, "Se esperaba un alias para el modulo");
            alias = my_strdup(tk.valor);
            adv();
        }
        return crear_nodo_importar(path, alias, NULL, 0);
    }
    
    if (tk.tipo == TOKEN_C_INCLUIR) {
        adv();
        if (tk.tipo != TOKEN_CADENA) lanzar_error(ERROR_SINTAXIS, "Se esperaba una cadena con el header tras c_incluir");
        NodoAST* n = crear_nodo(AST_C_INCLUIR);
        n->datos.c_incluir.header = my_strdup(tk.valor);
        adv();
        return n;
    }

    if (tk.tipo == TOKEN_C_EXTERN) {
        adv();
        if (tk.tipo != TOKEN_FUNCION) lanzar_error(ERROR_SINTAXIS, "Se esperaba 'funcion' tras c_extern");
        adv();
        if (tk.tipo != TOKEN_IDENTIFICADOR) lanzar_error(ERROR_SINTAXIS, "Se esperaba nombre de funcion externa");
        char* name = my_strdup(tk.valor);
        adv();
        
        if (tk.tipo != TOKEN_PAR_IZQ) lanzar_error(ERROR_SINTAXIS, "Se esperaba '('");
        adv();
        
        char** p_names = NULL;
        char** p_types = NULL;
        int np = 0;
        
        while (tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
            if (tk.tipo != TOKEN_IDENTIFICADOR) lanzar_error(ERROR_SINTAXIS, "Se esperaba nombre de argumento");
            p_names = realloc(p_names, sizeof(char*) * (np + 1));
            p_types = realloc(p_types, sizeof(char*) * (np + 1));
            p_names[np] = my_strdup(tk.valor);
            p_types[np] = my_strdup("int"); // Default type
            adv();
            
            if (tk.tipo == TOKEN_DOS_PUNTOS) {
                adv();
                if (tk.tipo == TOKEN_IDENTIFICADOR || tk.tipo == TOKEN_CADENA) {
                    free(p_types[np]);
                    p_types[np] = my_strdup(tk.valor);
                    adv();
                }
            }
            np++;
            if (tk.tipo == TOKEN_COMA) adv();
        }
        if (tk.tipo == TOKEN_PAR_DER) adv();
        
        char* ret = my_strdup("void");
        if (tk.tipo == TOKEN_DOS_PUNTOS) {
            adv();
            if (tk.tipo == TOKEN_IDENTIFICADOR) {
                free(ret);
                ret = my_strdup(tk.valor);
                adv();
            }
        }
        
        NodoAST* n = crear_nodo(AST_C_EXTERN);
        n->datos.c_extern.nombre = name;
        n->datos.c_extern.n_params = np;
        n->datos.c_extern.param_nombres = p_names;
        n->datos.c_extern.param_tipos = p_types;
        n->datos.c_extern.retorno = ret;
        return n;
    }

    if (tk.tipo == TOKEN_DESDE) {
        adv();
        if (tk.tipo != TOKEN_CADENA) lanzar_error(ERROR_SINTAXIS, "Se esperaba una cadena con la ruta del modulo tras 'desde'");
        char* path = my_strdup(tk.valor);
        adv();
        if (tk.tipo != TOKEN_IMPORTAR) lanzar_error(ERROR_SINTAXIS, "Se esperaba 'importar' tras la ruta del modulo");
        adv();
        
        char** nombres = NULL;
        int n_nombres = 0;
        
        while (tk.tipo == TOKEN_IDENTIFICADOR) {
            nombres = realloc(nombres, sizeof(char*) * (n_nombres + 1));
            nombres[n_nombres++] = my_strdup(tk.valor);
            adv();
            if (tk.tipo == TOKEN_COMA) adv();
            else break;
        }
        
        if (n_nombres == 0) lanzar_error(ERROR_SINTAXIS, "Se esperaba al menos un nombre para importar");
        
        return crear_nodo_importar(path, NULL, nombres, n_nombres);
    }

    if (tk.tipo == TOKEN_SEA) {
        adv();
        char name[MAX_ID_LEN]; strcpy(name, tk.valor); adv();
        TipoExacto te = TEX_AUTO;
        if (tk.tipo == TOKEN_DOS_PUNTOS) {
            adv(); te = string_a_tipo_exacto(tk.valor); adv();
        }
        NodoAST* val_node = NULL;
        if (tk.tipo == TOKEN_IGUAL) {
            adv();
            val_node = logica();
        }
        NodoAST* n = crear_nodo_asignacion(name, val_node);
        n->tipo_ex = te;
        if (exportar) n->es_publico = true;
        return n;
    }
    else if (tk.tipo == TOKEN_ENUM) {
        adv();
        char name[MAX_ID_LEN]; strcpy(name, tk.valor); adv();
        if (tk.tipo == TOKEN_LLAVE_IZQ) adv();
        
        char** v_names = NULL;
        int* v_fields = NULL;
        int n_v = 0;
        
        while (tk.tipo != TOKEN_LLAVE_DER && tk.tipo != TOKEN_EOF) {
            v_names = realloc(v_names, sizeof(char*) * (n_v + 1));
            v_fields = realloc(v_fields, sizeof(int) * (n_v + 1));
            
            v_names[n_v] = my_strdup(tk.valor); adv();
            int n_f = 0;
            if (tk.tipo == TOKEN_PAR_IZQ) {
                adv();
                while (tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
                    adv();
                    n_f++;
                    if (tk.tipo == TOKEN_COMA) adv();
                }
                if (tk.tipo == TOKEN_PAR_DER) adv();
            }
            v_fields[n_v++] = n_f;
            if (tk.tipo == TOKEN_COMA) adv();
        }
        if (tk.tipo == TOKEN_LLAVE_DER) adv();
        NodoAST* n = crear_nodo_enum(name, v_names, v_fields, n_v);
        if (exportar) n->es_publico = true;
        return n;
    }
    else if (tk.tipo == TOKEN_INTENTAR) {
        adv();
        NodoAST* bloque_try = parse_block();
        char var_error[MAX_ID_LEN]; var_error[0] = 0;
        NodoAST* bloque_catch = NULL;
        NodoAST* bloque_finally = NULL;

        if (tk.tipo == TOKEN_CAPTURAR) {
            adv();
            if (tk.tipo == TOKEN_PAR_IZQ) {
                adv();
                strcpy(var_error, tk.valor);
                adv();
                if (tk.tipo == TOKEN_PAR_DER) adv();
            }
            bloque_catch = parse_block();
        }

        if (tk.tipo == TOKEN_FINALMENTE) {
            adv();
            bloque_finally = parse_block();
        }

        return crear_nodo_try_catch(bloque_try, var_error, bloque_catch, bloque_finally);
    }
    else if (tk.tipo == TOKEN_LANZAR) {
        adv();
        NodoAST* expr = logica();
        return crear_nodo_lanzar(expr);
    }
    else if (tk.tipo == TOKEN_SI) {
        adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        NodoAST* cond = logica();
        if (tk.tipo == TOKEN_PAR_DER) adv();
        NodoAST* bloque_si = parse_block();
        NodoAST* bloque_sino = NULL;
        if (tk.tipo == TOKEN_SINO) {
            adv();
            bloque_sino = parse_block();
        }
        return crear_nodo_si(cond, bloque_si, bloque_sino, NULL, NULL, 0);
    }
    else if (tk.tipo == TOKEN_MIENTRAS) {
        adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        NodoAST* cond = logica();
        if (tk.tipo == TOKEN_PAR_DER) adv();
        NodoAST* cuerpo = parse_block();
        return crear_nodo_mientras(cond, cuerpo);
    }
    else if (tk.tipo == TOKEN_PARA) {
        adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        char var[MAX_ID_LEN]; strcpy(var, tk.valor); adv();
        
        if (tk.tipo == TOKEN_EN) adv();
        
        if (tk.tipo == TOKEN_IDENTIFICADOR && !strcmp(tk.valor, "rango")) adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        
        NodoAST* inicio = logica(); if(tk.tipo == TOKEN_COMA) adv();
        NodoAST* fin = logica();
        
        NodoAST* paso = NULL;
        if (tk.tipo == TOKEN_COMA) {
            adv();
            paso = logica();
        }
        if (tk.tipo == TOKEN_PAR_DER) adv();
        if (tk.tipo == TOKEN_PAR_DER) adv();
        
        NodoAST* cuerpo = parse_block();
        return crear_nodo_para(var, inicio, fin, paso, cuerpo);
    }
    else if (tk.tipo == TOKEN_RETORNAR) {
        adv();
        NodoAST* val = logica();
        NodoAST* n = crear_nodo(AST_RETORNAR);
        n->datos.unop.operando = val;
        return n;
    }
    else if (tk.tipo == TOKEN_CLASE) {
        adv();
        char name[MAX_ID_LEN]; strcpy(name, tk.valor); adv();
        char* padre = NULL;
        if (tk.tipo == TOKEN_HEREDA) {
            adv();
            padre = my_strdup(tk.valor);
            adv();
        }
        if (tk.tipo == TOKEN_LLAVE_IZQ) adv();
        
        NodoAST** miembros = NULL;
        int n_miembros = 0;
        int cap = 10;
        miembros = malloc(sizeof(NodoAST*) * cap);
        
        while (tk.tipo != TOKEN_LLAVE_DER && tk.tipo != TOKEN_EOF) {
            bool estatico = false;
            bool privado = false;
            while (tk.tipo == TOKEN_ESTATICO || tk.tipo == TOKEN_PRIVADO || tk.tipo == TOKEN_PUBLICO) {
                if (tk.tipo == TOKEN_ESTATICO) estatico = true;
                if (tk.tipo == TOKEN_PRIVADO) privado = true;
                adv();
            }
            
            NodoAST* m = parse_stmt();
            if (m) {
                m->es_estatico = estatico;
                m->es_privado = privado;
                if (n_miembros >= cap) {
                    cap *= 2;
                    miembros = realloc(miembros, sizeof(NodoAST*) * cap);
                }
                miembros[n_miembros++] = m;
            }
        }
        if (tk.tipo == TOKEN_LLAVE_DER) adv();
        NodoAST* n = crear_nodo_clase(name, padre, miembros, n_miembros);
        if (exportar) n->es_publico = true;
        return n;
    }
    else if (tk.tipo == TOKEN_IDENTIFICADOR) {
        char name[MAX_ID_LEN]; strcpy(name, tk.valor);
        int start_pos = ts->current;
        Token start_tk = tk;
        
        adv();
        if (tk.tipo == TOKEN_IGUAL) {
            adv();
            return crear_nodo_asignacion(name, logica());
        }
        else if (tk.tipo == TOKEN_PUNTO) {
            adv();
            char prop[MAX_ID_LEN]; strcpy(prop, tk.valor); adv();
            if (tk.tipo == TOKEN_IGUAL) {
                adv();
                NodoAST* val = logica();
                NodoAST* n = crear_nodo_asignacion(prop, val);
                n->datos.asignacion.receptor = crear_nodo_identificador(name);
                return n;
            } else {
                ts->current = start_pos;
                tk = start_tk;
                return logica();
            }
        }
        else if (tk.tipo == TOKEN_CORCHETE_IZQ) {
            adv();
            NodoAST* idx = logica();
            if(tk.tipo == TOKEN_CORCHETE_DER) adv();
            if(tk.tipo == TOKEN_IGUAL) {
                adv();
                NodoAST* val = logica();
                NodoAST* n = crear_nodo(AST_ASIGNACION);
                n->datos.binop.izquierda = crear_nodo_binop(crear_nodo_identificador(name), TOKEN_CORCHETE_IZQ, idx);
                n->datos.binop.derecha = val;
                n->tipo = AST_ASIGNACION;
                return n;
            }
        }
        
        ts->current = start_pos;
        tk = start_tk;
        return logica();
    }
    
    else if (tk.tipo == TOKEN_PRINT) {
        adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        NodoAST** args = NULL;
        int n_args = 0;
        while(tk.tipo != TOKEN_PAR_DER && tk.tipo != TOKEN_EOF) {
            args = realloc(args, sizeof(NodoAST*) * (n_args + 1));
            args[n_args++] = logica();
            if(tk.tipo == TOKEN_COMA) adv();
        }
        if (tk.tipo == TOKEN_PAR_DER) adv();
        return crear_nodo_llamada("print", args, n_args);
    }
    else if (tk.tipo == TOKEN_FUNCION) {
        adv();
        char name[MAX_ID_LEN]; strcpy(name, tk.valor); adv();
        if (tk.tipo == TOKEN_PAR_IZQ) adv();
        char** params = NULL;
        TipoExacto* param_tipos = NULL;
        int n_params = 0;
        while(tk.tipo == TOKEN_IDENTIFICADOR) {
            params = realloc(params, sizeof(char*) * (n_params + 1));
            param_tipos = realloc(param_tipos, sizeof(TipoExacto) * (n_params + 1));
            params[n_params] = my_strdup(tk.valor); adv();
            TipoExacto te = TEX_AUTO;
            if (tk.tipo == TOKEN_DOS_PUNTOS) {
                adv(); te = string_a_tipo_exacto(tk.valor); adv();
            }
            param_tipos[n_params++] = te;
            if (tk.tipo == TOKEN_COMA) adv();
        }
        if (tk.tipo == TOKEN_PAR_DER) adv();
        TipoExacto ret_tipo = TEX_AUTO;
        if (tk.tipo == TOKEN_DOS_PUNTOS) {
            adv(); ret_tipo = string_a_tipo_exacto(tk.valor); adv();
        }
        NodoAST* cuerpo = parse_block();
        NodoAST* n = crear_nodo_funcion(name, params, param_tipos, n_params, cuerpo, ret_tipo);
        if (exportar) n->es_publico = true;
        return n;
    }
    
    return logica();
}
