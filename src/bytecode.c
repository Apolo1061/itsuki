#include "itsuki.h"

#define MAGIC "SUKIBY"

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

void guardar_bytecode(Chunk* chunk, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        printf("Error: No se pudo crear el archivo %s\n", filename);
        return;
    }

    fwrite(MAGIC, 1, 6, f);

    fwrite(&chunk->contador_constantes, sizeof(int), 1, f);

    for (int i = 0; i < chunk->contador_constantes; i++) {
        Result c = chunk->constantes[i];
        fwrite(&c.tipo, sizeof(TipoDato), 1, f);
        if (c.tipo == TIPO_NUMERO) {
            fwrite(&c.n, sizeof(double), 1, f);
        } else if (c.tipo == TIPO_CADENA) {
            int len = strlen(c.s);
            fwrite(&len, sizeof(int), 1, f);
            fwrite(c.s, 1, len, f);
        }
    }

    fwrite(&chunk->contador, sizeof(int), 1, f);
    fwrite(chunk->codigo, 1, chunk->contador, f);

    fclose(f);
}

Chunk* cargar_bytecode(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    char magic[7];
    if (fread(magic, 1, 6, f) != 6 || strncmp(magic, MAGIC, 6) != 0) {
        printf("Error: %s no es un archivo de bytecode Itsuki valido\n", filename);
        fclose(f);
        return NULL;
    }

    Chunk* chunk = malloc(sizeof(Chunk));
    init_chunk(chunk);

    int n_const;
    if (fread(&n_const, sizeof(int), 1, f) != 1) {
        printf("Error: Fallo al leer cantidad de constantes\n");
        fclose(f);
        return NULL;
    }
    for (int i = 0; i < n_const; i++) {
        Result c;
        if (fread(&c.tipo, sizeof(TipoDato), 1, f) != 1) {
            printf("Error: Fallo al leer tipo de constante\n");
            fclose(f);
            return NULL;
        }
        if (c.tipo == TIPO_NUMERO) {
            if (fread(&c.n, sizeof(double), 1, f) != 1) {
                printf("Error: Fallo al leer valor numerico\n");
                fclose(f);
                return NULL;
            }
        } else if (c.tipo == TIPO_CADENA) {
            int len;
            if (fread(&len, sizeof(int), 1, f) != 1) {
                printf("Error: Fallo al leer longitud de cadena\n");
                fclose(f);
                return NULL;
            }
            c.s = malloc(len + 1);
            if (fread(c.s, 1, len, f) != (size_t)len) {
                printf("Error: Fallo al leer contenido de cadena\n");
                free(c.s);
                fclose(f);
                return NULL;
            }
            c.s[len] = 0;
            Result gc_s = gc_new_string(c.s);
            free(c.s);
            c = gc_s;
        }
        agregar_constante(chunk, c);
    }

    int n_inst;
    if (fread(&n_inst, sizeof(int), 1, f) != 1) {
        printf("Error: Fallo al leer cantidad de instrucciones\n");
        fclose(f);
        return NULL;
    }
    chunk->capacidad = n_inst;
    chunk->contador = n_inst;
    chunk->codigo = malloc(n_inst);
    if (fread(chunk->codigo, 1, n_inst, f) != (size_t)n_inst) {
        printf("Error: Fallo al leer codigo de instrucciones\n");
        free(chunk->codigo);
        fclose(f);
        return NULL;
    }

    fclose(f);
    preparar_cache_chunk(chunk);
    return chunk;
}
