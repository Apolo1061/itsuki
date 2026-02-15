#include "itsuki.h"

void init_chunk(Chunk* chunk) {
    chunk->capacidad = 0;
    chunk->contador = 0;
    chunk->codigo = NULL;
    chunk->constantes = NULL;
    chunk->capacidad_constantes = 0;
    chunk->contador_constantes = 0;
}

void free_chunk(Chunk* chunk) {
    if (chunk->codigo) free(chunk->codigo);
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

 

static bool obtener_valor_constante(NodoAST* n, double* out) {
    if (!n) return false;
    if (n->tipo == AST_NUMERO) {
        *out = n->datos.numero;
        return true;
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
            Result v = {0};
            v.tipo = TIPO_CADENA;
            v.s = my_strdup(n->datos.cadena);
            v.obj = NULL;
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
            compilar_nodo(chunk, n->datos.asignacion.valor);
            if (n->datos.asignacion.receptor) {
                // Asignación de propiedad: obj.prop = val
                compilar_nodo(chunk, n->datos.asignacion.receptor);
                Result prop = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.asignacion.nombre)};
                int idx = agregar_constante(chunk, prop);
                escribir_chunk(chunk, OP_ESTABLECER_PROPIEDAD);
                escribir_chunk(chunk, (uint8_t)idx);
            } else {
                if (n->es_publico) escribir_chunk(chunk, OP_MARCAR_EXPORT);
                Result nombre = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.asignacion.nombre)};
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
            Result nombre = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.identificador)};
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
            Result nombre = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.para.var_nombre)};
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
                    Result err_name = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.try_catch.var_error)};
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
            Result path_v = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.importar.path)};
            int path_idx = agregar_constante(chunk, path_v);
            
            int alias_idx = 0xff;
            if (n->datos.importar.alias) {
                Result alias_v = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.importar.alias)};
                alias_idx = agregar_constante(chunk, alias_v);
            }
            
            escribir_chunk(chunk, OP_IMPORTAR);
            escribir_chunk(chunk, (uint8_t)path_idx);
            escribir_chunk(chunk, (uint8_t)alias_idx);
            escribir_chunk(chunk, (uint8_t)n->datos.importar.n_nombres);
            
            for (int i=0; i < n->datos.importar.n_nombres; i++) {
                Result name_v = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.importar.nombres[i])};
                int name_idx = agregar_constante(chunk, name_v);
                escribir_chunk(chunk, (uint8_t)name_idx);
            }
            break;
        }

        case AST_LLAMADA: {
            for (int i=0; i < n->datos.llamada.n_args; i++) compilar_nodo(chunk, n->datos.llamada.args[i]);
            
            if (n->es_estatico) {
                Result nombre = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.llamada.nombre)};
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_SUPER);
                escribir_chunk(chunk, (uint8_t)idx);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else if (!strcmp(n->datos.llamada.nombre, "print")) {
                escribir_chunk(chunk, OP_PRINT);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else if (es_funcion_builtin(n->datos.llamada.nombre)) {
                Result nombre = {0};
                nombre.tipo = TIPO_CADENA;
                nombre.s = my_strdup(n->datos.llamada.nombre);
                int idx = agregar_constante(chunk, nombre);
                escribir_chunk(chunk, OP_OBTENER_GLOBAL);
                escribir_chunk(chunk, (uint8_t)idx);
                
                escribir_chunk(chunk, OP_LLAMAR);
                escribir_chunk(chunk, (uint8_t)n->datos.llamada.n_args);
            } else {
                Result nombre = {0};
                nombre.tipo = TIPO_CADENA;
                nombre.s = my_strdup(n->datos.llamada.nombre);
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
            
            n->datos.funcion.cuerpo = NULL; 
            hash_insert(&ht_funcs, funcs[f_idx].nombre, f_idx);

            if (n->datos.funcion.n_decoradores > 0) {
                Result nombre_f_res = {.tipo=TIPO_CADENA, .s=my_strdup(n->datos.funcion.nombre)};
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
            Result name = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.enumm.nombre)};
            int idx = agregar_constante(chunk, name);
            escribir_chunk(chunk, OP_ENUM);
            escribir_chunk(chunk, (uint8_t)idx);
            escribir_chunk(chunk, (uint8_t)n->datos.enumm.n_variantes);
            
            for (int i = 0; i < n->datos.enumm.n_variantes; i++) {
                Result v_name = {.tipo = TIPO_CADENA, .s = my_strdup(n->datos.enumm.variantes[i])};
                int v_idx = agregar_constante(chunk, v_name);
                escribir_chunk(chunk, (uint8_t)v_idx);
                escribir_chunk(chunk, (uint8_t)n->datos.enumm.n_campos_variante[i]);
            }
            break;
        }

        case AST_LAMBDA: {
            int f_idx = n_f++;
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
                Result nombre = {0};
                nombre.tipo = TIPO_CADENA;
                nombre.s = my_strdup(call->datos.llamada.nombre);
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

            Result nombre_val = {0};
            nombre_val.tipo = TIPO_CADENA;
            nombre_val.s = my_strdup(n->datos.clase.nombre);
            int idx_nombre = agregar_constante(chunk, nombre_val);
            
            int idx_padre = 0;
            if (n->datos.clase.padre) {
                Result padre_val = {0};
                padre_val.tipo = TIPO_CADENA;
                padre_val.s = my_strdup(n->datos.clase.padre);
                idx_padre = agregar_constante(chunk, padre_val) + 1;
            }
            
            escribir_chunk(chunk, OP_CLASE);
            escribir_chunk(chunk, (uint8_t)idx_nombre);
            escribir_chunk(chunk, (uint8_t)idx_padre);
            escribir_chunk(chunk, (uint8_t)n->datos.clase.n_miembros);
            
            for (int i=0; i < n->datos.clase.n_miembros; i++) {
                NodoAST* m = n->datos.clase.miembros[i];
                Result m_nombre = {0};
                m_nombre.tipo = TIPO_CADENA;
                if (m->tipo == AST_FUNCION) m_nombre.s = my_strdup(m->datos.funcion.nombre);
                else if (m->tipo == AST_ASIGNACION) m_nombre.s = my_strdup(m->datos.asignacion.nombre);
                
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
            Result nombre = {0};
            nombre.tipo = TIPO_CADENA;
            nombre.s = my_strdup(n->datos.llamada.nombre);
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
            
            Result nombre = {0};
            nombre.tipo = TIPO_CADENA;
            nombre.s = my_strdup(n->datos.llamada.nombre);
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

            Result var_res = {.tipo=TIPO_CADENA, .s=my_strdup(n->datos.comprehension.variables[0])};
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
    compilar_nodo(chunk, nodo);
    escribir_chunk(chunk, OP_RETORNAR);
    return chunk;
}
