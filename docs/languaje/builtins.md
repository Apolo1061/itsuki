## **Referencia de Funciones Integradas** (Built-ins)

| Función | Ejemplo | Descripción |
| :--- | :--- | :--- |
| **Básicas** | | |
| `print` | `printf("Hola", 123)` | Imprime múltiples argumentos en consola. |
| `leer` | `sea n = leer()` | Lee una línea de la entrada estándar. |
| `largo` | `largo([1,2,3])` | Retorna la longitud de un array o string. |
| `tipo_de` | `tipo_de(4.5)` | Retorna el tipo de dato ("numero", "cadena", "array", etc.). |
| **Conversión** | | |
| `string` | `string(100)` | Convierte cualquier valor a representación string. |
| `int` | `int("123")` | Convierte un valor a número entero. |
| `float` | `float("12.5")` | Convierte un valor a número decimal (punto flotante). |
| `booleano` | `booleano(1)` | Convierte un valor a booleano (`verdadero` o `falso`). |
| `bool` | `bool(0)` | Alias de `booleano`. |
| **Cadenas** | | |
| `mayusculas` | `mayusculas("hola")` | Convierte una cadena a mayúsculas. |
| `minusculas` | `minusculas("HOLA")` | Convierte una cadena a minúsculas. |
| `subcadena` | `subcadena("itsuki", 0, 3)` | Retorna una parte de la cadena (cadena, inicio, fin). |
| `split` | `split("a,b,c", ",")` | Divide una cadena en un array usando un separador. |
| `join` | `join(["a","b"], "-")` | Une un array en una cadena usando un separador. |
| `replace` | `replace("hola", "o", "a")` | Reemplaza ocurrencias de una subcadena por otra. |
| `contiene` | `contiene("itsuki", "su")` | Verifica si una cadena contiene otra. |
| `empieza_con` | `empieza_con("abc", "a")` | Verifica si una cadena empieza con un prefijo. |
| `termina_con` | `termina_con("abc", "c")` | Verifica si una cadena termina con un sufijo. |
| `repetir` | `repetir("ha", 3)` | Repite una cadena N veces. |
| **Arrays** | | |
| `agregar` | `agregar(arr, val)` | Añade un elemento al final de un array. |
| `quitar` | `quitar(arr, indice)` | Elimina un elemento en la posición especificada. |
| `reverso` | `reverso([1,2,3])` | Invierte el orden de los elementos en el array. |
| `ordenar` | `ordenar([3,1,2])` | Ordena los elementos del array (actualmente números). |
| **Matemáticas** | | |
| `abs` | `abs(-10)` | Retorna el valor absoluto de un número. |
| `potencia` | `potencia(2, 3)` | Eleva un número a la potencia especificada. |
| `min` | `min(5, 2, 8)` | Retorna el valor mínimo de una lista de argumentos. |
| `max` | `max(5, 2, 8)` | Retorna el valor máximo de una lista de argumentos. |
| `redondear` | `redondear(4.6)` | Redondea al entero más cercano. |
| `piso` | `piso(4.9)` | Redondea hacia abajo (suelo). |
| `techo` | `techo(4.1)` | Redondea hacia arriba (techo). |
| `aleatorio` | `aleatorio()` | Genera un número decimal aleatorio entre 0 y 1. |
| `aleatorio_rango`| `aleatorio_rango(1, 10)`| Genera un número aleatorio entre min y max. |
| **E/S y Archivos** | | |
| `leer_archivo` | `leer_archivo("t.txt")` | Lee el contenido completo de un archivo de texto. |
| `escribir_archivo`| `escribir_archivo("f.txt", "c")`| Escribe contenido en un archivo (sobrescribe). |
| `existe_archivo` | `existe_archivo("t.txt")` | Verifica si un archivo existe en el sistema. |
| **Sistema** | | |
| `dormir` | `dormir(1.5)` | Pausa la ejecución por N segundos. |
| `salir` | `salir(0)` | Finaliza la ejecución del programa con un código. |
| **Tipo Checking** | | |
| `es_numero` | `es_numero(10)` | Retorna `verdadero` si el valor es un número. |
| `es_cadena` | `es_cadena("a")` | Retorna `verdadero` si el valor es una cadena. |
| `es_array` | `es_array([])` | Retorna `verdadero` si el valor es un array. |
| **Alto Orden** | | |
| `filtrar` | `filtrar(arr, func)` | Crea un nuevo array con elementos que pasan un test. |
| `mapear` | `mapear(arr, func)` | Crea un nuevo array transformando cada elemento. |
| `cada` | `cada(arr, func)` | Ejecuta una función para cada elemento del array. |
| `reducir` | `reducir(arr, f, init)`| Reduce un array a un solo valor usando una función. |
