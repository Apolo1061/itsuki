## Gestor de Paquetes: itsuki-cy

`itsuki-cy` Su proposito es estandarizar como compartimos y consumimos codigo en la comunidad, eliminando la necesidad de copiar y pegar archivos manualmente entre proyectos.

### Filosofía y Diseño
- **Simplicidad ante todo**: No requiere cuentas ni servidores complejos. Si esta en GitHub, es instalable.
- **Directorio Local**: Al instalar un paquete, `itsuki-cy` lo coloca en la carpeta `modules/` de tu proyecto actual. Esto garantiza que tus dependencias esten aisladas y sean portátiles.
- **Transparencia**: Todo es código fuente. Puedes abrir la carpeta `modules/` y ver exactamente qué estás usando.

### Comandos en Detalle

| Comando | Operación Técnica | Ejemplo |
| :--- | :--- | :--- |
| `install <user/repo>` | Ejecuta `git clone` apuntando a `modules/repo`. | `./itsuki-cy install google/re2` |
| `list` | Lista los directorios dentro de `modules/`. | `./itsuki-cy list` |
| `remove <repo>` | Elimina el directorio `modules/<repo>`. | `./itsuki-cy remove re2` |
| `update <repo>` | Ejecuta `git pull` dentro del directorio del módulo. | `./itsuki-cy update re2` |

### Instalación de Versiones Específicas
Aunque el comando basico instala la rama por defecto, puedes navegar manualmente a `modules/<repo>` y usar comandos de Git (`git checkout tags/v1.0`) para fijar versiones. Itsuki respetara los archivos presentes en el disco.

### Solución de Problemas Comunes
- **"git" no se reconoce**: `itsuki-cy` requiere Git en el PATH del sistema. Instálalo desde [git-scm.com](https://git-scm.com/).
- **Error al clonar**: Verifica que el repositorio sea público o que tengas tus credenciales de SSH/HTTPS configuradas en tu terminal.
- **Módulos no encontrados**: Asegúrate de que estás ejecutando `itsuki.exe` desde la raíz del proyecto donde se encuentra la carpeta `modules/`.


---

## Guía de Creación de Bibliotecas (Módulos)

Crear una biblioteca para Itsuki es sencillo. Sigue estas reglas para que tus módulos sean compatibles y fáciles de usar por otros.

### 1. Estructura del Proyecto
Un módulo típico debe tener al menos un archivo principal (usualmente el nombre del repo o `init.suki`).

```text
mi_libreria/
├── init.suki
├── utilidades.suki
└── README.md
```

### 2. Exportación y Namespaces
En Itsuki, el encapsulamiento es vital. Todo lo que no tenga el prefijo `exportar` es local al archivo.

```itsuki
// modules/red/util.suki

// Visible para quien importe "red"
exportar clase Conexion { ... }

// Visible para quien importe "red"
exportar funcion ping() { ... }

// OCULTO: Solo accesible dentro de este archivo
sea _intentos_max = 3;
```

**Tip Pro**: Evita colisiones de nombres. Si tu librería se llama `json_utils`, prefiere exportar una única clase o un objeto que contenga todo, en lugar de muchas funciones sueltas.

### 3. El archivo `index.suki` o `init.suki`
Si tu librería tiene muchos archivos, crea un `init.suki` en la raíz de su carpeta que importe y re-exporte todo. Así, el usuario solo tiene que hacer:
`importar desde "mi_libreria" importar *;`

### 3. Uso de Módulos (Importación)
Para usar una biblioteca instalada con `itsuki-cy`:

```itsuki
// Los módulos en la carpeta 'modules' se buscan automáticamente
importar desde "matematica" importar sumar, PI;

escribir(sumar(10, PI));
```

### 4. Buenas Prácticas
- **Nombres Claros**: Usa nombres descriptivos para tus archivos de módulo.
- **Documentación**: Incluye un `README.md` explicando para qué sirve tu código.
- **Sin Efectos Secundarios**: Evita ejecutar código pesado en el nivel superior del módulo; prefiere envolverlo en funciones.
- **Encapsulamiento**: Usa el prefijo `_` para funciones internas (aunque no es obligatorio, es una convención).

---

---

## Módulos Nativos (C)

A veces necesitas el máximo rendimiento o interactuar con APIs de bajo nivel (como sockets o hardware). Itsuki te permite escribir funciones en C e integrarlas directamente.

### 1. Crear el archivo C
Crea un nuevo archivo en la carpeta `src/`, por ejemplo `src/mi_modulo.c`. Tus funciones deben seguir esta firma:

```c
#include "itsuki.h"

Result mi_funcion_nativa(Result args[], int n_args) {
    // tu logica aqui
    return gc_new_string("Resultado desde C");
}
```

### 2. Registrar la función
Actualmente, las funciones nativas deben registrarse manualmente en el motor:
1. Abre `src/builtins.c`.
2. Añade el prototipo al inicio o incluye tu archivo.
3. En `ejecutar_builtin`, añade el nombre de búsqueda:
   ```c
   if(!strcmp(nombre, "mi_funcion")) return mi_funcion_nativa(args, n_args);
   ```
4. En `registrar_builtins`, añade `"mi_funcion"` a la lista de nombres.

### 3. Compilar
Simplemente añade tu archivo al comando de compilación o utiliza el sistema de build automático de Itsuki:
`gcc -I./src/headers src/*.c -o itsuki.exe -lws2_32 -lm`

> [!TIP]
> Consulta el archivo de ejemplo en [src/extension_ejemplo.c](src/extension_ejemplo.c) para ver una implementacion de referencia con manejo de errores y argumentos.
