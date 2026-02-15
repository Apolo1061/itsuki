#include "itsuki.h"
#include "linter.h"

void imprimir_perfil() {
    printf("\n--- REPORTE DE PERFILADO ---\n");
    printf("%-30s | %-12s | %-10s\n", "Funcion", "Tiempo (s)", "Llamadas");
    printf("-------------------------------|--------------|-----------\n");
    
    for (int i=0; i<n_perfil-1; i++) {
        for (int j=0; j<n_perfil-i-1; j++) {
            if (perfil_datos[j].tiempo_total < perfil_datos[j+1].tiempo_total) {
                ProfileData temp = perfil_datos[j];
                perfil_datos[j] = perfil_datos[j+1];
                perfil_datos[j+1] = temp;
            }
        }
    }
    
    for (int i=0; i<n_perfil; i++) {
        printf("%-30s | %-12.6f | %-10ld\n", 
            perfil_datos[i].nombre_func, 
            perfil_datos[i].tiempo_total, 
            perfil_datos[i].llamadas);
    }
    printf("----------------------------------------------------------\n");
}

#ifndef ITSUKI_BUNDLED
bool es_funcion_nativa_proxy(const char* nombre) { return false; }
Result ejecutar_nativa_proxy(const char* nombre, Result args[], int n_args) {
    return (Result){.tipo = TIPO_NULO};
}
#endif

#ifndef ITS_EMBED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* path;
    char* content;
} BundledFile;

struct Array* cli_args = NULL;

BundledFile bundled_files[256];
int n_bundled = 0;
char* visited_paths[256];
int n_visited = 0;

bool ya_visitado(const char* path) {
    for (int i=0; i<n_visited; i++) {
        if (!strcmp(visited_paths[i], path)) return true;
    }
    return false;
}

typedef struct {
    char header[256];
} CInclude;

CInclude c_includes[64];
int n_c_includes = 0;

typedef struct {
    char name[64];
    char ret_type[64];
    char param_types[16][64]; // max 16 params
    int n_params;
} CExtern;

CExtern c_externs[128];
int n_c_externs = 0;

char* leer_archivo_texto(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        char p2[256]; sprintf(p2, "%s.suki", path);
        f = fopen(p2, "rb");
        if (!f) return NULL;
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* c = malloc(sz + 1);
    size_t nr = fread(c, 1, sz, f);
    if (nr != (size_t)sz) {
        free(c);
        fclose(f);
        return NULL;
    }
    c[sz] = 0;
    fclose(f);
    return c;
}

void add_c_include(const char* h) {
    for(int i=0; i<n_c_includes; i++) {
        if(!strcmp(c_includes[i].header, h)) return;
    }
    strcpy(c_includes[n_c_includes++].header, h);
}

void collect_dependencies(const char* path) {
    char clean_path[256]; strcpy(clean_path, path);
    if (ya_visitado(clean_path)) return;
    
    char* content = leer_archivo_texto(clean_path);
    if (!content) {
        printf("Advertencia: No se pudo leer modulo '%s' para bundling.\n", clean_path);
        return;
    }
    
    visited_paths[n_visited++] = my_strdup(clean_path);
    bundled_files[n_bundled].path = my_strdup(clean_path);
    bundled_files[n_bundled].content = content; 
    n_bundled++;
    
    TokenStream* ts_local = tokenize_all(content);
    if (!ts_local) return;
    
    int current = 0;
    Token local_tk;
    #define LOCAL_ADV() (local_tk = ts_local->tokens[current++])
    #define LOCAL_PEEK() (ts_local->tokens[current])
    
    LOCAL_ADV();
    while (local_tk.tipo != TOKEN_EOF) {
        if (local_tk.tipo == TOKEN_IMPORTAR || local_tk.tipo == TOKEN_DESDE) {
            LOCAL_ADV();
            if (local_tk.tipo == TOKEN_CADENA) {
                collect_dependencies(local_tk.valor);
            }
        } else if (local_tk.tipo == TOKEN_C_INCLUIR) {
            LOCAL_ADV();
             if (local_tk.tipo == TOKEN_CADENA) {
                add_c_include(local_tk.valor);
             }
        } else if (local_tk.tipo == TOKEN_C_EXTERN) {
            LOCAL_ADV();
            if(local_tk.tipo == TOKEN_FUNCION) LOCAL_ADV();
            if(local_tk.tipo == TOKEN_IDENTIFICADOR) {
                CExtern* ext = &c_externs[n_c_externs++];
                strcpy(ext->name, local_tk.valor);
                ext->n_params = 0;
                
                LOCAL_ADV();
                if(local_tk.tipo == TOKEN_PAR_IZQ) {
                    LOCAL_ADV();
                    while(local_tk.tipo != TOKEN_PAR_DER && local_tk.tipo != TOKEN_EOF) {
                        LOCAL_ADV(); 
                        if(local_tk.tipo == TOKEN_DOS_PUNTOS) {
                            LOCAL_ADV();
                           if(local_tk.tipo == TOKEN_IDENTIFICADOR || local_tk.tipo == TOKEN_CADENA) {
                                strcpy(ext->param_types[ext->n_params], local_tk.valor);
                            }
                        } else {
                            strcpy(ext->param_types[ext->n_params], "int");
                        }
                        ext->n_params++;
                        LOCAL_ADV();
                        if(local_tk.tipo == TOKEN_COMA) LOCAL_ADV();
                    }
                }
                
                strcpy(ext->ret_type, "void");
                LOCAL_ADV();
                if(local_tk.tipo == TOKEN_DOS_PUNTOS) {
                    LOCAL_ADV();
                    if(local_tk.tipo == TOKEN_IDENTIFICADOR) strcpy(ext->ret_type, local_tk.valor);
                }
            }
        }
        LOCAL_ADV();
    }
    
    free_token_stream(ts_local);
}

int main(int argc, char** argv) {
    if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf("Itsuki Scripting Engine v4.5\n");
        printf("Uso:\n");
        printf("  itsuki <script.suki>          Ejecuta un script\n");
        printf("  itsuki <binario.sukiby>       Ejecuta bytecode\n");
        printf("  itsuki <script.suki> -o <out> Compila a ejecutable nativo\n");
        printf("  itsuki <script.suki> -c <out> Genera codigo C nativo\n");
        printf("  itsuki <script.suki> -b <out> Compila a bytecode (.sukiby)\n");
        printf("  itsuki --lsp                  Inicia el servidor de lenguaje (LSP)\n");
        printf("  itsuki                        Inicia el modo REPL\n");
        return 0;
    }

    if (argc >= 2 && !strcmp(argv[1], "--lsp")) {
        lsp_start();
        return 0;
    }

    if (argc < 2) {
        printf("Itsuki REPL v4.5\n");
        printf("Escribe ':ayuda' para ver comandos o ':salir' para terminar.\n");
        itsuki_init();
        repl_mode = true;
        
        char buffer[1024];
        while(1) {
            printf("itsuki> ");
            if(!fgets(buffer, 1024, stdin)) break;
            
            buffer[strcspn(buffer, "\n")] = 0;
            
            if(!strcmp(buffer, ":salir") || !strcmp(buffer, ":exit")) break;
            if(!strcmp(buffer, ":ayuda")) {
                printf("Comandos:\n  :salir    Salir del REPL\n  :ayuda    Mostrar esta ayuda\n  :limpiar  Limpiar pantalla\n");
                continue;
            }
            if(!strcmp(buffer, ":limpiar") || !strcmp(buffer, ":cls")) {
                int res = system("cls");
                (void)res;
                continue;
            }
            
            if(strlen(buffer) == 0) continue;
            
            strcpy(ultima_linea_repl, buffer);
            
            if(setjmp(error_jmp) == 0) {
                itsuki_execute(buffer);
            } else {
            }
        }
        return 0;
    }

    if (argc >= 4 && (!strcmp(argv[2], "-o") || !strcmp(argv[2], "-c") || !strcmp(argv[2], "-b"))) {
        bool only_c = !strcmp(argv[2], "-c");
        bool compile_bc = !strcmp(argv[2], "-b");
        
        bool static_link = false;
        bool strip_bin = false;
        bool keep_temp = false;
        
        for(int i=4; i<argc; i++) {
            if(!strcmp(argv[i], "--static")) static_link = true;
            if(!strcmp(argv[i], "--strip")) strip_bin = true;
            if(!strcmp(argv[i], "--keep-temp")) keep_temp = true;
        }

        char final_out[256];
        strcpy(final_out, argv[3]);

        if (compile_bc && !strstr(final_out, ".sukiby")) {
            strcat(final_out, ".sukiby");
        }
        
        char* out_name = final_out;
        
        if (only_c) printf("Generando codigo C para %s -> %s...\n", argv[1], out_name);
        else if (compile_bc) printf("Compilando Bytecode %s -> %s...\n", argv[1], out_name);
        else printf("Compilando Itsuki v4.5 %s -> %s.exe...\n", argv[1], out_name);

        collect_dependencies(argv[1]);
        
        FILE* out_c = fopen(only_c ? out_name : "bundle.c", "w");
        
        fprintf(out_c, "#define ITS_EMBED \"");
        char* script = bundled_files[0].content;
        for(int i=0; script[i]; i++) {
            if(script[i] == '\"') fprintf(out_c, "\\\"");
            else if(script[i] == '\\') fprintf(out_c, "\\\\");
            else if(script[i] == '\n') fprintf(out_c, "\\n");
            else if(script[i] == '\r') fprintf(out_c, "\\r");
            else if(script[i] == '\t') fprintf(out_c, "\\t");
            else fputc(script[i], out_c);
        }
        fprintf(out_c, "\"\n");

        fprintf(out_c, "#define ITSUKI_BUNDLED\n");
        fprintf(out_c, "typedef struct { const char* path; const char* content; } BundledFile;\n");
        fprintf(out_c, "static BundledFile bundled_files[] = {\n");
        for(int i=0; i<n_bundled; i++) {
            fprintf(out_c, "    {.path = \"%s\", .content = \"", bundled_files[i].path);
            char* s = bundled_files[i].content;
            for(int j=0; s[j]; j++) {
                if(s[j] == '\"') fprintf(out_c, "\\\"");
                else if(s[j] == '\\') fprintf(out_c, "\\\\");
                else if(s[j] == '\n') fprintf(out_c, "\\n");
                else if(s[j] == '\r') fprintf(out_c, "\\r");
                else if(s[j] == '\t') fprintf(out_c, "\\t");
                else fputc(s[j], out_c);
            }
            fprintf(out_c, "\"},\n");
        }
        fprintf(out_c, "    {.path = 0, .content = 0}\n};\n");
        
        fprintf(out_c, "#include \"src/utils.c\"\n");
        fprintf(out_c, "#include \"src/lexer.c\"\n");
        fprintf(out_c, "#include \"src/gc.c\"\n");
        fprintf(out_c, "#include \"src/parser.c\"\n");
        fprintf(out_c, "#include \"src/evaluator.c\"\n");
        fprintf(out_c, "#include \"src/compiler.c\"\n"); 
        fprintf(out_c, "#include \"src/vm.c\"\n");       
        fprintf(out_c, "#include \"src/bytecode.c\"\n"); 
        fprintf(out_c, "#include \"src/builtins.c\"\n");
        fprintf(out_c, "#include \"src/socket.c\"\n");
        fprintf(out_c, "#include \"src/lsp.c\"\n");
        
        for(int i=0; i<n_c_includes; i++) {
            fprintf(out_c, "#include %s\n", c_includes[i].header);
        }

        for(int i=0; i<n_c_externs; i++) {
            CExtern* ext = &c_externs[i];
            fprintf(out_c, "Result glue_%s(Result args[], int n_args) {\n", ext->name);
            fprintf(out_c, "    if (n_args < %d) return (Result){.tipo = TIPO_NULO};\n", ext->n_params);
            
            bool returns_void = !strcmp(ext->ret_type, "void");
            if (!returns_void) fprintf(out_c, "    %s res_top = ", ext->ret_type);
            else fprintf(out_c, "    ");
            
            fprintf(out_c, "%s(", ext->name);
            for(int j=0; j<ext->n_params; j++) {
                if(!strcmp(ext->param_types[j], "int")) fprintf(out_c, "(int)args[%d].n", j);
                else if(!strcmp(ext->param_types[j], "double")) fprintf(out_c, "args[%d].n", j);
                else if(!strcmp(ext->param_types[j], "string") || !strcmp(ext->param_types[j], "char*")) fprintf(out_c, "args[%d].s ? args[%d].s : \"\"", j, j);
                else if(!strcmp(ext->param_types[j], "bool")) fprintf(out_c, "args[%d].n ? 1 : 0", j);
                else fprintf(out_c, "(int)args[%d].n", j);
                
                if(j < ext->n_params - 1) fprintf(out_c, ", ");
            }
            fprintf(out_c, ");\n");
            
            if (returns_void) {
                fprintf(out_c, "    return (Result){.tipo = TIPO_NULO};\n");
            } else {
                if(!strcmp(ext->ret_type, "int")) fprintf(out_c, "    return (Result){.n = (double)res_top, .tipo = TIPO_NUMERO};\n");
                else if(!strcmp(ext->ret_type, "double")) fprintf(out_c, "    return (Result){.n = res_top, .tipo = TIPO_NUMERO};\n");
                else if(!strcmp(ext->ret_type, "char*")) fprintf(out_c, "    return gc_new_string(res_top);\n");
                else if(!strcmp(ext->ret_type, "bool")) fprintf(out_c, "    return (Result){.n = res_top ? 1 : 0, .tipo = TIPO_BOOL};\n");
                else fprintf(out_c, "    return (Result){.tipo = TIPO_NULO};\n");
            }
            fprintf(out_c, "}\n");
        }
        
        fprintf(out_c, "bool es_funcion_nativa_proxy(const char* nombre) {\n");
        for(int i=0; i<n_c_externs; i++) {
            fprintf(out_c, "    if(!strcmp(nombre, \"%s\")) return true;\n", c_externs[i].name);
        }
        fprintf(out_c, "    return false;\n}\n");
        
        fprintf(out_c, "Result ejecutar_nativa_proxy(const char* nombre, Result args[], int n_args) {\n");
        for(int i=0; i<n_c_externs; i++) {
            fprintf(out_c, "    if(!strcmp(nombre, \"%s\")) return glue_%s(args, n_args);\n", c_externs[i].name, c_externs[i].name);
        }
        fprintf(out_c, "    return (Result){.tipo = TIPO_NULO};\n}\n");

        fprintf(out_c, "void registrar_ffi() {\n");
        fprintf(out_c, "    int f_idx;\n");
        for(int i=0; i<n_c_externs; i++) {
            fprintf(out_c, "    f_idx = n_f++;\n");
            fprintf(out_c, "    strcpy(funcs[f_idx].nombre, \"%s\");\n", c_externs[i].name);
            fprintf(out_c, "    funcs[f_idx].n_params = %d;\n", c_externs[i].n_params);
            fprintf(out_c, "    funcs[f_idx].chunk_bytecode = NULL;\n");
            fprintf(out_c, "    funcs[f_idx].cuerpo_ast = NULL;\n");
            fprintf(out_c, "    hash_insert(&ht_funcs, \"%s\", f_idx);\n", c_externs[i].name);
        }
        fprintf(out_c, "}\n");

        fprintf(out_c, "#define ITSUKI_FFI_BUNDLE\n");
        fprintf(out_c, "#include \"src/main.c\"\n");
        fprintf(out_c, "int main() { itsuki_init(); registrar_ffi(); itsuki_execute(ITS_EMBED); return 0; }\n");
        
        fclose(out_c);
        
        if (!only_c) {
            char cmd[512]; 
            sprintf(cmd, "gcc bundle.c -I./src/headers -O3 -march=native %s %s -o %s -lm",
                static_link ? "-static" : "",
                strip_bin ? "-s" : "",
                out_name
            );
            
            if(system(cmd) == 0) {
                printf("¡Exito! %s creado con optimizacion -O3 -march=native.\n", out_name);
                if (!keep_temp) remove("bundle.c");
                else printf("Info: bundle.c conservado (--keep-temp).\n");
            }
            else printf("Error en la compilacion nativa.\n");
        } else {
            printf("¡Exito! Archivo %s generado.\n", out_name);
        }
        return 0;
    }
    
    if (strstr(argv[1], ".sukic") || strstr(argv[1], ".sukiby")) {
    }

    if (!strcmp(argv[1], "fmt")) {
        if (argc < 3) {
            printf("Uso: itsuki fmt <archivo.suki>\n");
            return 1;
        }
        itsuki_fmt(argv[2]);
        return 0;
    }

    if (!strcmp(argv[1], "lint")) {
        if (argc < 3) {
            printf("Uso: itsuki lint <archivo.suki>\n");
            return 1;
        }
        FILE* f = fopen(argv[2], "rb");
        if (!f) { printf("Archivo no encontrado: %s\n", argv[2]); return 1; }
        fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
        char* b = malloc(s + 1); 
        size_t nr_lint = fread(b, 1, s, f); b[nr_lint] = 0; fclose(f);
        itsuki_init();
        itsuki_lint(b);
        free(b);
        return 0;
    }

    FILE* f = fopen(argv[1], "rb"); if(!f) { printf("Archivo no encontrado: %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc(s + 1); 
    size_t nr_main = fread(b, 1, s, f); b[nr_main] = 0; fclose(f);
    
    itsuki_init();
    bool do_lint = false;
    for (int i=2; i<argc; i++) {
        if (!strcmp(argv[i], "--debug")) { vm.debug_mode = true; vm.step_mode = true; }
        if (!strcmp(argv[i], "--profile")) vm.profiling_mode = true;
        if (!strcmp(argv[i], "--no-gc")) { vm.manual_memory_mode = true; }
        if (!strcmp(argv[i], "--lint")) do_lint = true;
    }
    
    cli_args = array_crear(argc - 2 > 0 ? argc - 2 : 0);
    for (int i=2; i<argc; i++) {
        array_agregar(cli_args, gc_new_string(argv[i]));
    }
    
    if (do_lint) {
        itsuki_lint(b);
        return 0;
    }
    
    itsuki_execute(b);
    if (vm.profiling_mode) imprimir_perfil();
    return 0;
}
#else
#ifndef ITSUKI_FFI_BUNDLE
int main() { main_exec(ITS_EMBED); return 0; }
#endif
#endif
