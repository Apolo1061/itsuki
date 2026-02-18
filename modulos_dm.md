# Modulos nativos de Itsuki (C) - Guia Windows y Linux

Version objetivo: **Itsuki v5.0**

Este documento explica como crear un modulo nativo en C para Itsuki, igual al estilo de `modules/math`.

## 1. Idea general

Un modulo nativo:

- Se escribe en C.
- Exporta una funcion obligatoria: `itsuki_module_init(...)`.
- Se compila como libreria compartida:
  - Windows: `.dll`
  - Linux: `.so`
- Se importa desde Itsuki con `importar "modules/tu_modulo" como m`.

## 2. Requisitos

- Tener `src/headers/itsuki_ext.h` y `src/headers/itsuki_types.h`.
- Compilador C (`gcc`).
- Runtime de Itsuki con soporte de modulos nativos (loader de `.dll/.so`).

## 3. Estructura recomendada

```text
itsuki/
  modules/
    mi_modulo.c
    mi_modulo.dll   (Windows)
    mi_modulo.so    (Linux)
```

## 4. Plantilla minima de modulo C

```c
#include "itsuki_ext.h"

static const ItsukiApi* g_api = NULL;

static int es_num(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

static Result fn_doble(Result args[], int n_args) {
    if (n_args < 1 || !es_num(args[0])) {
        g_api->raise(ERROR_ARGUMENTO, "doble(x) requiere 1 numero");
        return g_api->make_null();
    }
    return g_api->make_number(args[0].n * 2.0);
}

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;

    g_api = api;

    if (!api->export_const(module, "VERSION", api->make_string("1.0.0"))) return 0;
    if (!api->export_native(module, "doble", fn_doble)) return 0;

    return 1;
}
```

## 5. Compilar en Windows

Desde la raiz del proyecto:

```bash
gcc -shared -O3 -Wall -I./src/headers modules/mi_modulo.c -o modules/mi_modulo.dll -lm
```

Con Makefile (recomendado):

```bash
cd modules
make mi_modulo.dll
# o para compilar todos:
make
```

## 6. Compilar en Linux

Desde la raiz del proyecto:

```bash
gcc -shared -fPIC -O3 -Wall -I./src/headers modules/mi_modulo.c -o modules/mi_modulo.so -lm
```

Con Makefile (recomendado):

```bash
cd modules
make mi_modulo.so
# o para compilar todos:
make
```

## 7. Usar el modulo desde Itsuki

```suki
importar "modules/mi_modulo" como mm

print(mm.VERSION)
print(mm.doble(21))
```

## 8. API util (resumen)

Desde `ItsukiApi`:

- Crear valores:
  - `make_number`
  - `make_bool`
  - `make_null`
  - `make_string`
  - `make_array`
  - `make_map`
- Contenedores:
  - `array_push`
  - `map_set`
- Exportar:
  - `export_const`
  - `export_native`
- Error:
  - `raise`

Firma de funciones nativas:

```c
typedef Result (*ItsukiNativeFn)(Result args[], int n_args);
```

## 9. Errores comunes

- `No se pudo abrir el modulo 'modules/mi_modulo'`
  - Falta compilar `.dll/.so`, o nombre/ruta incorrecta.
- `no exporta itsuki_module_init`
  - Tu modulo no define la funcion de entrada obligatoria.
- Importa `.suki` en vez del binario
  - Si existe `modules/mi_modulo.suki`, ese archivo puede tener prioridad en algunas configuraciones.
- Tu `itsuki.exe` no carga nativos
  - Recompila el core asegurando que incluya `src/itsuki_ext_runtime.c`.

## 10. Recomendaciones de rendimiento

- Evita conversiones/string innecesarias dentro de loops.
- Valida argumentos una sola vez.
- Reusa buffers locales cuando se pueda.
- Para operaciones pesadas sobre arrays, hazlas completas en C y devuelve un solo resultado.
