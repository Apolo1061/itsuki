#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void print_help() {
    printf("Itsuki Package Manager\n");
    printf("Uso: itsuki-cy <comando> [argumentos]\n\n");
    printf("Comandos:\n");
    printf("  install <user/repo>  Instala un paquete desde GitHub\n");
    printf("  remove <repo>        Elimina un paquete del directorio modules/\n");
    printf("  update <repo | all>    Actualiza un paquete o todos los instalados\n");
    printf("  list                 Lista los paquetes instalados\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    char* command = argv[1];

    if (strcmp(command, "list") == 0) {
        printf("Listando paquetes en modules/...\n");
        int r = system("dir modules /B"); (void)r;
    } else if (strcmp(command, "remove") == 0) {
        if (argc < 3) {
            printf("Error: remove requiere el nombre del paquete\n");
            return 1;
        }
        char cmd[512];
#ifdef _WIN32
        sprintf(cmd, "rmdir /S /Q modules\\%s", argv[2]);
#else
        sprintf(cmd, "rm -rf modules/%s", argv[2]);
#endif
        printf("Eliminando paquete: %s\n", argv[2]);
        int r = system(cmd); (void)r;
    } else if (strcmp(command, "update") == 0) {
        if (argc < 3) {
            printf("Error: update requiere el nombre del paquete o 'all'\n");
            return 1;
        }

        if (strcmp(argv[2], "all") == 0) {
            printf("Actualizando todos los paquetes en modules/...\n");
#ifdef _WIN32
            WIN32_FIND_DATA fd;
            HANDLE h = FindFirstFile("modules\\*", &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                            char cmd[512];
                            sprintf(cmd, "cd modules\\%s && git pull", fd.cFileName);
                            printf("\n--- Actualizando: %s ---\n", fd.cFileName);
                            int r = system(cmd); (void)r;
                        }
                    }
                } while (FindNextFile(h, &fd));
                FindClose(h);
            }
#else
            int r = system("for dir in modules/*; do if [ -d \"$dir\" ]; then echo \"\n--- Actualizando: ${dir#modules/} ---\"; (cd \"$dir\" && git pull); fi; done"); (void)r;
#endif
        } else {
            char cmd[512];
#ifdef _WIN32
            sprintf(cmd, "cd modules\\%s && git pull", argv[2]);
#else
            sprintf(cmd, "cd modules/%s && git pull", argv[2]);
#endif
            printf("Actualizando paquete: %s\n", argv[2]);
            int r = system(cmd); (void)r;
        }
    } else {
        printf("Comando desconocido: %s\n", command);
        print_help();
    }

    return 0;
}
