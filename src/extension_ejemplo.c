#include "itsuki.h"

Result builtin_hola_c(Result args[], int n_args) {
    printf("¡Hola desde el mundo de C!\n");
    
    if (n_args > 0 && args[0].tipo == TIPO_CADENA) {
        printf("Recibi: %s\n", args[0].s);
    }

    Result res = {0};
    res.tipo = TIPO_NULO;
    return res;
}

Result builtin_sumar_rapido(Result args[], int n_args) {
    if (n_args < 2) {
        lanzar_error(ERROR_ARGUMENTO, "sumar_rapido() requiere 2 numeros");
    }
    
    double a = args[0].n;
    double b = args[1].n;
    
    Result res = {0};
    res.tipo = TIPO_NUMERO;
    res.n = a + b;
    return res;
}
