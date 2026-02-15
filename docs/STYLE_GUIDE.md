# Guía de Estilo de Itsuki

Esta guia define convenciones para escribir codigo idiomatico y legible en Itsuki. Esta pensada para proyectos, ejemplos y contribuciones al ecosistema.

## Principios
- Legibilidad ante todo: prioriza claridad sobre trucos y concision extrema.
- Consistencia: sigue las mismas reglas en todo el archivo y proyecto.

## Formato
- Indentacion: 4 espacios por nivel. No mezcles tabs y espacios.
- Longitud de línea: intenta no superar ~100 caracteres.
- Llaves:
  - Apertura en la misma línea.
  - Cierre alineado con el inicio del bloque.
- Espacios:
  - Alrededor de operadores: `a + b`, `x == y`.
  - Después de comas: `funcion f(a, b, c)`.
  - No antes de `,` o `;`.
- Líneas en blanco:
  - 1 línea en blanco entre funciones y definiciones de clase.
  - Dentro de bloques, separa grupos lógicos con moderación.
- Punto y coma:
  - Evítalo; solo úsalo si necesitas varias sentencias en una línea.

## Comentarios
- En scripts Itsuki usa `#` para comentarios de línea.
- `//` y `/* ... */` no forman parte de la sintaxis de comentarios en Itsuki (se usan en el código C del motor).
- Escribe comentarios breves y al punto. Evita repetir lo que ya dice el código.
- Encabezados de sección en archivos largos:
  ```
  # --- Utilidades de cadena ---
  ```

## Nombres
- Variables y funciones: `snake_case` en español. Ej: `contador_total`, `calcular_media`.
- Clases y Enums: `PascalCase`. Ej: `Persona`, `EstadoConexion`.
- Miembros privados: `privado` con nombre claro, sin prefijos. Ej: `privado contraseña`.
- Constantes y macros del preprocesador: `UPPER_SNAKE_CASE`. Ej: `%definir TAMANO_BUFFER 4096`.
- Parámetros: cortos pero descriptivos. Evita una sola letra fuera de bucles.

## Variables y Tipado
- Declara con `sea` y asigna explícitamente cuando ayude a la claridad:
  ```
  sea total = 0
  ```
- Usa tipos exactos del preprocesador solo cuando sea necesario (rendimiento, FFI, binarios):
  ```
  %int32 TAM = 1024
  ```

## Funciones
- Declaración:
  ```
  funcion sumar(a, b) {
      retornar a + b
  }
  ```
- Retornos: usa `retornar` siempre; evita retornos implícitos.
- Lambdas: úsalas para callbacks simples; prefiere funciones con nombre en lógica compleja.

## Estructuras de Control
- Condicionales:
  ```
  si (condicion) {
      # ...
  } sino {
      # ...
  }
  ```
- Bucles:
  ```
  mientras (seguir) { ... }
  para (i en rango(0, 10)) { ... }
  ```
- Palabras lógicas: `and`, `or`, `not` (o `y`, `o`, `no` según versión admitida), sé consistente.

## Clases y POO
- Definición:
  ```
  clase Persona {
      publico nombre
      privado edad
      estatico especie = "Humano"
      
      funcion constructor(nombre, edad) {
          this.nombre = nombre
          this.edad = edad
      }
      
      funcion presentar() {
          imprimir(f"Soy {this.nombre}")
      }
  }
  ```
- Herencia:
  ```
  clase Empleado hereda Persona { ... }
  ```
- Instancias:
  ```
  sea p = nueva Persona("Ana", 30)
  ```
- Visibilidad: usa `publico` y `privado`. Miembros `estatico` para datos/funciones de clase.

## Enums y Patrones
- Definición:
  ```
  enum Resultado {
      Exito
      Error
  }
  ```
- Comprobación:
  ```
  si (r es Resultado.Exito) { ... }
  ```
- Usa `caso`/`es` donde la sintaxis lo permita para distinguir variantes.

## Módulos
- Importar/exportar:
  ```
  exportar funcion util()
  
  importar "lib/strings" como str
  desde "math/avanzada" como m
  ```
- Nombra archivos y módulos en `snake_case`. Evita rutas relativas frágiles.

## Manejo de Errores
- Usa `intentar / capturar / finalmente` y `lanzar`:
  ```
  intentar {
      operar()
  } capturar (e) {
      imprimir(f"Error: {e}")
  } finalmente {
      limpiar()
  }
  ```
- Lanza errores con mensajes claros y en español.

## Cadenas y Texto
- Cadenas dobles `"texto"`.
- f-strings:
  ```
  imprimir(f"Usuario: {nombre}, Edad: {edad}")
  ```
- Evita concatenación excesiva; prefiere f-strings o `join`.

## Entrada/Salida
- Salida: usa `print` o `imprimir` según la versión disponible; sé consistente en el proyecto.
- Entrada: usa `leer("Mensaje: ")` con mensajes claros y cortos.

## Preprocesador
- Directivas soportadas (ejemplos): `%macro`, `%definir`, `%if`, `%else`, `%endif`, y tipos exactos (`%int32`, `%float32`, `%bool`, etc.).
- Estilo:
  - Macros en `UPPER_SNAKE_CASE`.
  - Evita lógica compleja en el preprocesador.

## Estilo de Colecciones
- Arrays: `[1, 2, 3]` con espacio tras comas.
- Mapas/dict: `{ clave: valor }` con un espacio tras `:` y tras `,`.
- Comprensiones: usa solo si mejora la claridad.

## Convenciones de Proyecto
- Un archivo por módulo/tema principal.
- Tests y ejemplos cortos, autoexplicativos y en español.
- Evita dependencias implícitas; usa `importar`/`exportar`.

## Ejemplo Completo
```
# Demo de estilo en Itsuki

funcion sumar(a, b) {
    retornar a + b
}

funcion main() {
    sea a = leer("Ingresa el primer número: ")
    sea b = leer("Ingresa el segundo número: ")
    print(f"Resultado: {sumar(a, b)}")
}

main()
```

## Compatibilidad y Versiones
- Algunas palabras clave o built-ins pueden variar (ej. `print`/`imprimir`, `input`/`leer`). Manten coherencia dentro del proyecto y documenta las decisiones.

---
Aplica esta guia con criterio. Si una regla no encaja en un caso concreto, prioriza la legibilidad y explica la excepción.

