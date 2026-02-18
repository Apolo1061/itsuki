#include "itsuki.h"

Variable vars[MAX_VARS]; 
int n_v = 0;
Funcion funcs[MAX_FUNCS]; 
int n_f = 0;
int ln = 1;
Lexer lx; 
Token tk;

HashTable ht_vars;
HashTable ht_funcs;
TokenStream* ts = NULL;
StringPool* pool = NULL;
Result last_res = {0};
bool returning = false;
bool debe_romper = false;
bool debe_continuar = false;
HashNode* current_scope_nodes = NULL;

void adv() { 
    if (ts && ts->current < ts->count) {
        tk = ts->tokens[ts->current++];
        ln = tk.linea;
    } else {
        tk = next_tk(&lx); 
    }
}

void leap() {
    int v = 1; 
    adv();
    while (v > 0 && tk.tipo != TOKEN_EOF) {
        if (tk.tipo == TOKEN_LLAVE_IZQ) v++;
        if (tk.tipo == TOKEN_LLAVE_DER) v--;
        adv();
    }
}

void free_token_stream(TokenStream* stream) {
    if(!stream) return;
    for(int i=0; i<stream->count; i++) {
        if(stream->tokens[i].valor && 
           (stream->tokens[i].tipo == TOKEN_IDENTIFICADOR ||
            stream->tokens[i].tipo == TOKEN_NUMERO ||
            stream->tokens[i].tipo == TOKEN_CADENA ||
            stream->tokens[i].tipo == TOKEN_FSTRING)) {
            free(stream->tokens[i].valor);
        }
    }
    free(stream->tokens);
    free(stream);
}

TokenStream* tokenize_all(const char* source) {
    TokenStream* stream = malloc(sizeof(TokenStream));
    stream->capacity = 1024;
    stream->tokens = malloc(sizeof(Token) * stream->capacity);
    stream->count = 0;
    stream->current = 0;
    
    Lexer l;
    memset(&l, 0, sizeof(Lexer));
    l.f = source; l.p = 0;
    Token t;
    int saved_ln = ln;
    ln = 1;
    
    do {
        t = next_tk(&l);
        if (stream->count >= stream->capacity) {
            stream->capacity *= 2;
            stream->tokens = realloc(stream->tokens, sizeof(Token) * stream->capacity);
        }
        stream->tokens[stream->count++] = t;
    } while (t.tipo != TOKEN_EOF);
    
    ln = saved_ln;
    return stream;
}

Token next_tk(Lexer* l) {
    while (l->f[l->p]) {
        while (l->f[l->p] && isspace(l->f[l->p])) {
            if (l->f[l->p] == '\n') ln++;
            l->p++;
        }
        if (l->f[l->p] == '%') {
            int p_orig = l->p; l->p++;
            char dir[64]; int di=0;
            while(isalnum(l->f[l->p]) && di < 63) {
                dir[di++] = l->f[l->p++];
            }
            dir[di]=0;
            if(!strcmp(dir, "macro")) {
                while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                    l->p++;
                }
                char cmd[64]; int ci=0;
                while(isalnum(l->f[l->p]) && ci < 63) {
                    cmd[ci++] = l->f[l->p++];
                }
                cmd[ci]=0;
                
                bool currently_skipping = false;
                if (l->pp_depth > 0 && !l->pp_stack[l->pp_depth-1].is_active) currently_skipping = true;

                if (!strcmp(cmd, "if") || !strcmp(cmd, "ifdef") || !strcmp(cmd, "ifndef")) {
                    bool is_ifndef = !strcmp(cmd, "ifndef");
                    while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    char name[64]; int ni=0;
                    while(isalnum(l->f[l->p]) || l->f[l->p] == '_') {
                        name[ni++] = l->f[l->p++];
                    }
                    name[ni]=0;
                    
                    bool cond = false;
                    Macro* m = macro_lookup(name);
                    if (m) {
                        if (strcmp(m->reemplazo, "0") && strcmp(m->reemplazo, "false")) cond = true;
                    }
                    if (is_ifndef) cond = !cond;

                    if (l->pp_depth < 32) {
                        bool parent_active = (l->pp_depth == 0) ? true : l->pp_stack[l->pp_depth-1].is_active;
                        l->pp_stack[l->pp_depth].condition_met = cond;
                        l->pp_stack[l->pp_depth].is_active = parent_active && cond;
                        l->pp_depth++;
                    }
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                } else if (!strcmp(cmd, "elif")) {
                    if (l->pp_depth > 0) {
                        bool parent_active = (l->pp_depth == 1) ? true : l->pp_stack[l->pp_depth-2].is_active;
                        bool already_met = l->pp_stack[l->pp_depth-1].condition_met;
                        
                        while(isspace(l->f[l->p]) && l->f[l->p] != '\n') l->p++;
                        char name[64]; int ni=0;
                        while(isalnum(l->f[l->p]) || l->f[l->p] == '_') {
                            name[ni++] = l->f[l->p++];
                        }
                        name[ni]=0;
                        
                        bool cond = false;
                        Macro* m = macro_lookup(name);
                        if (m) {
                             if (strcmp(m->reemplazo, "0") && strcmp(m->reemplazo, "false")) cond = true;
                        }

                        if (!already_met && parent_active && cond) {
                            l->pp_stack[l->pp_depth-1].is_active = true;
                            l->pp_stack[l->pp_depth-1].condition_met = true;
                        } else {
                            l->pp_stack[l->pp_depth-1].is_active = false;
                        }
                    }
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                } else if (!strcmp(cmd, "else")) {
                    if (l->pp_depth > 0) {
                        bool parent_active = (l->pp_depth == 1) ? true : l->pp_stack[l->pp_depth-2].is_active;
                        bool was_met = l->pp_stack[l->pp_depth-1].condition_met;
                        l->pp_stack[l->pp_depth-1].is_active = parent_active && !was_met;
                        l->pp_stack[l->pp_depth-1].condition_met = true; 
                    }
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                } else if (!strcmp(cmd, "endif")) {
                    if (l->pp_depth > 0) {
                        l->pp_depth--;
                    }
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                } else if (!strcmp(cmd, "undef")) {
                    if (!currently_skipping) {
                        while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                            l->p++;
                        }
                        char name[64]; int ni=0;
                        while(isalnum(l->f[l->p]) || l->f[l->p] == '_') {
                            name[ni++] = l->f[l->p++];
                        }
                        name[ni]=0;
                        int idx = hash_lookup(&ht_macros, name);
                        if (idx != -1) {
                            macros[idx].nombre[0] = 0; 
                        }
                    }
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                } else if (!currently_skipping) {
                    char name[64]; TipoExacto te = TEX_AUTO;
                    if (!strcmp(cmd, "definir")) {
                        while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                            l->p++;
                        }
                        int ni=0;
                        while(isalnum(l->f[l->p]) || l->f[l->p] == '_') {
                            name[ni++] = l->f[l->p++];
                        }
                        name[ni]=0;
                    } else {
                        te = string_a_tipo_exacto(cmd);
                        if (te != TEX_AUTO) {
                            while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                                l->p++;
                            }
                            int ni=0;
                            while(isalnum(l->f[l->p]) || l->f[l->p] == '_') {
                                name[ni++] = l->f[l->p++];
                            }
                            name[ni]=0;
                        } else {
                            strcpy(name, cmd);
                        }
                    }
                    while(isspace(l->f[l->p]) && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    int start_val = l->p;
                    while(l->f[l->p] && l->f[l->p] != '\n' && l->f[l->p] != '\r') {
                        l->p++;
                    }
                    int len_val = l->p - start_val;
                    while(len_val > 0 && isspace(l->f[start_val + len_val - 1])) {
                        len_val--;
                    }
                    char val[256]; strncpy(val, l->f + start_val, len_val); val[len_val] = 0;
                    macro_insert(name, val, te);
                    continue;
                } else {
                    while(l->f[l->p] && l->f[l->p] != '\n') {
                        l->p++;
                    }
                    continue;
                }
            } else {
                l->p = p_orig;
            }
        }
        if (l->f[l->p] == '#') {
            while (l->f[l->p] && l->f[l->p] != '\n') l->p++;
            continue;
        }

        if (l->pp_depth > 0 && !l->pp_stack[l->pp_depth-1].is_active) {
            if (l->f[l->p] == '\n') ln++;
            if (l->f[l->p]) l->p++;
            continue;
        }
        break;
    }
    if (!l->f[l->p]) return (Token){TOKEN_EOF, "EOF", ln};
    int start = l->p; char c = l->f[l->p];
    if (c == 'f' && l->f[l->p+1] == '"') {
        l->p += 2;
        char buf[4096]; int bi = 0;
        while (l->f[l->p] && l->f[l->p] != '"') {
            if (l->f[l->p] == '\\') {
                l->p++;
                if (l->f[l->p] == 'n') buf[bi++] = '\n';
                else if (l->f[l->p] == 'r') buf[bi++] = '\r';
                else if (l->f[l->p] == 't') buf[bi++] = '\t';
                else if (l->f[l->p] == '"') buf[bi++] = '"';
                else if (l->f[l->p] == '\\') buf[bi++] = '\\';
                else buf[bi++] = l->f[l->p];
            } else buf[bi++] = l->f[l->p];
            l->p++;
        }
        buf[bi]=0; if(l->f[l->p] == '"') l->p++;
        return (Token){TOKEN_FSTRING, my_strdup(buf), ln};
    }

    if (isalpha(c) || c == '_') {
        while (isalnum(l->f[l->p]) || l->f[l->p] == '_') l->p++;
        int len = l->p - start; char b[64]; strncpy(b, l->f+start, len); b[len]=0;
        
        Macro* m = macro_lookup(b);
        if (m) {
            char* re = m->reemplazo;
            if (isdigit(re[0]) || (re[0] == '-' && isdigit(re[1]))) {
                if (m->tipo_ex != TEX_AUTO) {
                    Result r = {.n = atof(re), .tipo = TIPO_NUMERO};
                    aplicar_limites_tipo(&r, m->tipo_ex);
                    char buf[64]; sprintf(buf, "%g", r.n);
                    return (Token){TOKEN_NUMERO, my_strdup(buf), ln};
                }
                return (Token){TOKEN_NUMERO, my_strdup(re), ln};
            }
            if (re[0] == '"') {
                char* clean = my_strdup(re + 1); if(clean[strlen(clean)-1] == '"') clean[strlen(clean)-1] = 0;
                return (Token){TOKEN_CADENA, clean, ln};
            }
            if (!strcmp(re, "sea")) return (Token){TOKEN_SEA, my_strdup(re), ln};
            return (Token){TOKEN_IDENTIFICADOR, my_strdup(re), ln};
        }

        if (!strcmp(b, "sea")) return (Token){TOKEN_SEA, my_strdup(b), ln};
        if (!strcmp(b, "let")) return (Token){TOKEN_LET, my_strdup(b), ln};
        if (!strcmp(b, "var")) return (Token){TOKEN_VAR, my_strdup(b), ln};
        if (!strcmp(b, "const")) return (Token){TOKEN_CONST, my_strdup(b), ln};
        if (!strcmp(b, "print")) return (Token){TOKEN_PRINT, my_strdup(b), ln};
        if (!strcmp(b, "input")) return (Token){TOKEN_LEER, my_strdup(b), ln};
        if (!strcmp(b, "funcion") || !strcmp(b, "function")) return (Token){TOKEN_FUNCION, my_strdup(b), ln};
        if (!strcmp(b, "importar") || !strcmp(b, "import")) return (Token){TOKEN_IMPORTAR, my_strdup(b), ln};
        if (!strcmp(b, "exportar") || !strcmp(b, "export")) return (Token){TOKEN_EXPORTAR, my_strdup(b), ln};
        if (!strcmp(b, "desde") || !strcmp(b, "from")) return (Token){TOKEN_DESDE, my_strdup(b), ln};
        if (!strcmp(b, "como") || !strcmp(b, "as")) return (Token){TOKEN_COMO, my_strdup(b), ln};
        if (!strcmp(b, "retornar") || !strcmp(b, "return")) return (Token){TOKEN_RETORNAR, my_strdup(b), ln};
        if (!strcmp(b, "and")) return (Token){TOKEN_Y, my_strdup(b), ln};
        if (!strcmp(b, "or")) return (Token){TOKEN_O, my_strdup(b), ln};
        if (!strcmp(b, "not")) return (Token){TOKEN_NO, my_strdup(b), ln};
        if (!strcmp(b, "si") || !strcmp(b, "if")) return (Token){TOKEN_SI, my_strdup(b), ln};
        if (!strcmp(b, "sino") || !strcmp(b, "else")) return (Token){TOKEN_SINO, my_strdup(b), ln};
        if (!strcmp(b, "mientras") || !strcmp(b, "while")) return (Token){TOKEN_MIENTRAS, my_strdup(b), ln};
        if (!strcmp(b, "para") || !strcmp(b, "for")) return (Token){TOKEN_PARA, my_strdup(b), ln};
        if (!strcmp(b, "en") || !strcmp(b, "in")) return (Token){TOKEN_EN, my_strdup(b), ln};
        if (!strcmp(b, "romper") || !strcmp(b, "break")) return (Token){TOKEN_ROMPER, my_strdup(b), ln};
        if (!strcmp(b, "continuar") || !strcmp(b, "continue")) return (Token){TOKEN_CONTINUAR, my_strdup(b), ln};
        if (!strcmp(b, "c_incluir")) return (Token){TOKEN_C_INCLUIR, my_strdup(b), ln};
        if (!strcmp(b, "c_extern")) return (Token){TOKEN_C_EXTERN, my_strdup(b), ln};
        
        if (!strcmp(b, "intentar") || !strcmp(b, "try")) return (Token){TOKEN_INTENTAR, my_strdup(b), ln};
        if (!strcmp(b, "capturar") || !strcmp(b, "catch")) return (Token){TOKEN_CAPTURAR, my_strdup(b), ln};
        if (!strcmp(b, "finalmente") || !strcmp(b, "finally")) return (Token){TOKEN_FINALMENTE, my_strdup(b), ln};
        if (!strcmp(b, "lanzar") || !strcmp(b, "throw")) return (Token){TOKEN_LANZAR, my_strdup(b), ln};
        if (!strcmp(b, "clase") || !strcmp(b, "class")) return (Token){TOKEN_CLASE, my_strdup(b), ln};
        if (!strcmp(b, "nueva") || !strcmp(b, "new")) return (Token){TOKEN_NUEVA, my_strdup(b), ln};
        if (!strcmp(b, "hereda") || !strcmp(b, "extends")) return (Token){TOKEN_HEREDA, my_strdup(b), ln};
        if (!strcmp(b, "estatico") || !strcmp(b, "static")) return (Token){TOKEN_ESTATICO, my_strdup(b), ln};
        if (!strcmp(b, "privado") || !strcmp(b, "private")) return (Token){TOKEN_PRIVADO, my_strdup(b), ln};
        if (!strcmp(b, "publico") || !strcmp(b, "public")) return (Token){TOKEN_PUBLICO, my_strdup(b), ln};
        if (!strcmp(b, "super")) return (Token){TOKEN_SUPER, my_strdup(b), ln};
        if (!strcmp(b, "lambda")) return (Token){TOKEN_LAMBDA, my_strdup(b), ln};
        if (!strcmp(b, "nulo") || !strcmp(b, "null")) return (Token){TOKEN_NULO, my_strdup(b), ln};
        if (!strcmp(b, "verdadero") || !strcmp(b, "true")) return (Token){TOKEN_VERDADERO, my_strdup(b), ln};
        if (!strcmp(b, "falso") || !strcmp(b, "false")) return (Token){TOKEN_FALSO, my_strdup(b), ln};
        if (!strcmp(b, "enum")) return (Token){TOKEN_ENUM, my_strdup(b), ln};
        if (!strcmp(b, "caso") || !strcmp(b, "case")) return (Token){TOKEN_CASO, my_strdup(b), ln};
        if (!strcmp(b, "es") || !strcmp(b, "is")) return (Token){TOKEN_ES, my_strdup(b), ln};
        return (Token){TOKEN_IDENTIFICADOR, my_strdup(b), ln};
    } else if (isdigit(c)) {
        while (isdigit(l->f[l->p]) || l->f[l->p] == '.') l->p++;
        int len = l->p - start; char b[64]; strncpy(b, l->f+start, len); b[len]=0;
        return (Token){TOKEN_NUMERO, my_strdup(b), ln};
    } else if (c == '"') {
        l->p++;
        char buf[4096]; int bi = 0;
        while (l->f[l->p] && l->f[l->p] != '"') {
            if (l->f[l->p] == '\\') {
                l->p++;
                if (l->f[l->p] == 'n') buf[bi++] = '\n';
                else if (l->f[l->p] == 'r') buf[bi++] = '\r';
                else if (l->f[l->p] == 't') buf[bi++] = '\t';
                else if (l->f[l->p] == '"') buf[bi++] = '"';
                else if (l->f[l->p] == '\\') buf[bi++] = '\\';
                else buf[bi++] = l->f[l->p];
            } else buf[bi++] = l->f[l->p];
            l->p++;
        }
        buf[bi]=0; if(l->f[l->p] == '"') l->p++;
        return (Token){TOKEN_CADENA, my_strdup(buf), ln};
    } else {
        l->p++;
        if (c == '<') {
            if (l->f[l->p] == '=') { l->p++; return (Token){TOKEN_MENOR_IGUAL, "<=", ln}; }
            return (Token){TOKEN_MENOR, "<", ln};
        }
        if (c == '>') {
            if (l->f[l->p] == '=') { l->p++; return (Token){TOKEN_MAYOR_IGUAL, ">=", ln}; }
            return (Token){TOKEN_MAYOR, ">", ln};
        }
        if (c == '=') {
            if (l->f[l->p] == '=') { l->p++; return (Token){TOKEN_IGUAL_IGUAL, "==", ln}; }
            return (Token){TOKEN_IGUAL, "=", ln};
        }
        if (c == '!') {
            if (l->f[l->p] == '=') { l->p++; return (Token){TOKEN_DIFERENTE, "!=", ln}; }
            return (Token){TOKEN_NO, "!", ln};
        }
        if (c == '&' && l->f[l->p] == '&') { l->p++; return (Token){TOKEN_Y, "&&", ln}; }
        if (c == '|' && l->f[l->p] == '|') { l->p++; return (Token){TOKEN_O, "||", ln}; }

        if (c == '(') return (Token){TOKEN_PAR_IZQ, "(", ln}; 
        if (c == ')') return (Token){TOKEN_PAR_DER, ")", ln};
        if (c == '{') return (Token){TOKEN_LLAVE_IZQ, "{", ln}; 
        if (c == '}') return (Token){TOKEN_LLAVE_DER, "}", ln};
        if (c == '[') return (Token){TOKEN_CORCHETE_IZQ, "[", ln};
        if (c == ']') return (Token){TOKEN_CORCHETE_DER, "]", ln};
        if (c == '@') return (Token){TOKEN_ARROBA, "@", ln};
        if (c == ':') return (Token){TOKEN_DOS_PUNTOS, ":", ln};
        if (c == '.') return (Token){TOKEN_PUNTO, ".", ln};
        if (c == '+') return (Token){TOKEN_MAS, "+", ln};
        if (c == '-') return (Token){TOKEN_MENOS, "-", ln}; 
        if (c == '*') return (Token){TOKEN_MULT, "*", ln};
        if (c == '/') return (Token){TOKEN_DIV, "/", ln}; 
        if (c == '%') return (Token){TOKEN_MODULO, "%", ln};
        if (c == ',') return (Token){TOKEN_COMA, ",", ln};
        if (c == ';') return (Token){TOKEN_PUNTO_COMA, ";", ln};
    }
    return (Token){TOKEN_ERROR, "Err", ln};
}
