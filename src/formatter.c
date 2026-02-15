#include "itsuki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void imprimir_indentacion(FILE* out, int nivel) {
    for (int i = 0; i < nivel * 4; i++) {
        fputc(' ', out);
    }
}

void itsuki_fmt(const char* filepath) {
    FILE* in = fopen(filepath, "r");
    if (!in) {
        printf("Error: No se pudo abrir el archivo %s\n", filepath);
        return;
    }

    char temp_path[512];
    sprintf(temp_path, "%s.tmp", filepath);
    FILE* out = fopen(temp_path, "w");
    if (!out) {
        printf("Error: No se pudo crear el archivo temporal\n");
        fclose(in);
        return;
    }

    char line[1024];
    int nivel = 0;

    while (fgets(line, sizeof(line), in)) {
        char* start = line;
        while (*start && isspace(*start)) start++;
        
        char* end = start + strlen(start) - 1;
        while (end > start && isspace(*end)) {
            *end = '\0';
            end--;
        }

        if (*start == '\0') {
            fprintf(out, "\n");
            continue;
        }

        bool empieza_cierre = (*start == '}' || *start == ']');
        int nivel_linea = nivel;
        if (empieza_cierre && nivel_linea > 0) nivel_linea--;

        imprimir_indentacion(out, nivel_linea);

        for (char* p = start; *p; p++) {
            if (*p == '"' && (p == start || *(p-1) != '\\')) {
            }

            if (*p == '{' || *p == '[') nivel++;
            if (*p == '}' || *p == ']') {
                if (nivel > 0) nivel--;
            }
            
            fputc(*p, out);
        }
        fputc('\n', out);
    }

    fclose(in);
    fclose(out);

    if (remove(filepath) != 0) {
        perror("Error al eliminar archivo original");
    }
    if (rename(temp_path, filepath) != 0) {
        perror("Error al renombrar archivo temporal");
        printf("El archivo formateado quedó en: %s\n", temp_path);
    } else {
        printf("¡Exito! Archivo %s formateado correctamente.\n", filepath);
    }
}
