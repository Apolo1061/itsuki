# Arquitectura Interna de Itsuki: El Motor de Ejecución

Este documento detalla el funcionamiento interno del lenguaje Itsuki, desde que se lee el código fuente hasta que se ejecuta en la máquina virtual (VM). La arquitectura de Itsuki está diseñada para ser modular, eficiente y extensible, utilizando un pipeline de compilación híbrido.

## 1. El Pipeline de Ejecución (Vista General)

El proceso de ejecución de un script de Itsuki sigue estos pasos fundamentales:

1.  **Lectura del Código**: El archivo `.itsuki` se carga en memoria como una cadena de texto.
2.  **Lexer (Analizador Léxico)**: Convierte la cadena en una secuencia de tokens.
3.  **Parser (Analizador Sintáctico)**: Organiza los tokens en un Árbol de Sintaxis Abstracta (AST).
4.  **Compilador de Bytecode**: Transforma el AST en instrucciones de bajo nivel (Bytecode).
5.  **Máquina Virtual (VM)**: Ejecuta el bytecode de forma secuencial utilizando una pila.
6.  **Garbage Collector (GC)**: Gestiona la memoria de los objetos creados durante la ejecución.

---

## 2. Analizador Léxico (Lexer)

El Lexer es la primera etapa. Su trabajo es identificar "palabras" y "símbolos" válidos.

-   **Tokens**: Itsuki define una amplia variedad de tokens en [itsuki.h](file:///c:/Users/Mary%20Jimenez/Desktop/itsuki/src/headers/itsuki.h#L30-L62), como `TOKEN_SEA`, `TOKEN_FUNCION`, `TOKEN_NUMERO`, etc.
-   **Procesamiento**: El lexer salta espacios en blanco y comentarios, identifica números (incluyendo decimales), cadenas de texto (entre comillas) e identificadores (nombres de variables o funciones).
-   **Preprocesador**: Itsuki incluye directivas de preprocesador como `%macro` y `%ifdef`. Estas se procesan a nivel de lexer, permitiendo la compilación condicional antes de que el parser vea los tokens.

---

## 3. Analizador Sintáctico (Parser)

El Parser toma la lista de tokens y verifica que sigan las reglas gramaticales del lenguaje.

-   **AST (Abstract Syntax Tree)**: El resultado es una estructura jerárquica donde cada nodo representa una operación (una suma, una definición de función, un bucle `mientras`). Los nodos están definidos en la estructura `NodoAST`.
-   **Recursión Descendente**: Itsuki utiliza un parser de descenso recursivo. Funciones como `parse_stmt()`, `parse_expr()` y `parse_block()` se llaman unas a otras para construir el árbol.
-   **Manejo de Errores**: Si el parser encuentra un token inesperado (por ejemplo, `sea 5 = x`), lanza un error de sintaxis indicando la línea exacta.

---

## 4. El Sistema de Tipos (Estatico-Dinámico)

Itsuki es un lenguaje de tipado dinámico por defecto, pero permite anotaciones de tipo opcionales para mayor control y optimización.

-   **TipoDato**: Define los tipos de alto nivel como `TIPO_NUMERO`, `TIPO_CADENA`, `TIPO_ARRAY`, `TIPO_INSTANCIA`, etc.
-   **TipoExacto**: Permite especificar tipos de bajo nivel similares a C (e.g., `TEX_INT32`, `TEX_FLOAT64`). Esto es crucial para la interoperabilidad con C a través de `c_extern`.

---

## 5. Compilación a Bytecode

A diferencia de los intérpretes simples que recorren el AST directamente, Itsuki compila el AST a **Bytecode** para mejorar el rendimiento.

-   **Opcodes**: Las instrucciones de la VM se definen como `OpCode` (e.g., `OP_SUMA`, `OP_LLAMAR`, `OP_SALTAR`).
-   **Chunk**: Un "Chunk" es un bloque de bytecode que contiene el código ejecutable y una tabla de constantes (números y strings utilizados en el script).
-   **Saltos y Control**: Las estructuras de control (`si`, `mientras`) se implementan mediante instrucciones de salto (`OP_SALTAR`, `OP_SALTAR_SI_FALSO`), permitiendo que la VM mueva el puntero de instrucción (IP) eficientemente.

---

## 6. La Máquina Virtual (VM) de Pila

La VM es el corazón de Itsuki. Implementa una arquitectura basada en pila, lo que simplifica la generación de código y la ejecución de expresiones.

-   **Pila de Datos**: Los valores se apilan y desapilan. Para sumar `5 + 3`, la VM hace:
    1. `OP_CONSTANTE 5` (Empuja 5 a la pila)
    2. `OP_CONSTANTE 3` (Empuja 3 a la pila)
    3. `OP_SUMA` (Saca 5 y 3, suma, y empuja 8)
-   **Gestión de Locales**: Las variables locales se almacenan directamente en la pila de la VM, lo que permite un acceso extremadamente rápido mediante offsets.
-   **Clausuras (Closures)**: Itsuki soporta funciones de orden superior y clausuras. La VM utiliza `Upvalues` para permitir que las funciones internas accedan a variables de sus funciones padre, incluso después de que estas hayan terminado de ejecutarse.

---

## 7. Gestión de Memoria (Garbage Collector)

Itsuki utiliza un recolector de basura de tipo **Mark-and-Sweep** (Marcar y Limpiar) para evitar fugas de memoria.

-   **Objetos**: Todos los datos complejos (strings, arrays, mapas, instancias de clases) heredan de una estructura base `Obj`.
-   **Fase de Marcado**: El GC recorre todas las variables activas (globales y locales en la pila de la VM) y marca los objetos que aún son alcanzables.
-   **Fase de Limpieza**: El motor recorre la lista global de todos los objetos creados y libera la memoria de aquellos que no fueron marcados.
-   **Umbral Dinámico**: El GC se activa automáticamente cuando la memoria utilizada supera un umbral, el cual se ajusta dinámicamente según el uso del programa.

---

## 8. Programación Orientada a Objetos (POO)

El motor implementa una POO avanzada con soporte para:

-   **Clases e Instancias**: Las clases definen propiedades y métodos. Las instancias almacenan el estado específico.
-   **Herencia**: Las clases pueden heredar de otras, permitiendo la reutilización de código y el polimorfismo.
-   **Visibilidad**: Soporte para miembros `privado`, `publico` y `estatico`.
-   **Metaprogramación**: Itsuki permite inspeccionar objetos en tiempo de ejecución y llamar a métodos dinámicamente.

---

## 9. Interoperabilidad con C (FFI)

Una de las características más potentes de Itsuki es su capacidad para llamar a funciones de C directamente.

-   **c_extern**: Permite declarar una firma de función de C dentro de Itsuki. El motor se encarga de convertir los tipos de Itsuki a los tipos nativos de C y viceversa.
-   **c_incluir**: Permite incluir encabezados de C para que el compilador subyacente pueda enlazar las bibliotecas necesarias.

---

## 10. Módulos y Encapsulación

Itsuki utiliza un sistema de módulos inspirado en lenguajes modernos.

-   **Importación**: Los módulos se cargan y ejecutan una sola vez. Sus exportaciones se almacenan en un objeto de tipo `TIPO_MODULO`.
-   **Exportación**: La palabra clave `exportar` (o `TOKEN_EXPORTAR`) marca variables o clases como disponibles para otros scripts.

---

## Resumen Técnico de Estructuras

| Componente | Archivo Principal | Función Clave |
| :--- | :--- | :--- |
| **Lexer** | `lexer.c` | `next_tk()` |
| **Parser** | `parser.c` | `parse_stmt()` |
| **Evaluador/VM** | `evaluator.c` / `vm.c` | `evaluar_ast()` / `vm_ejecutar()` |
| **Built-ins** | `builtins.c` | `ejecutar_builtin()` |
| **GC** | `memory.c` | `collect_garbage()` |

Esta arquitectura permite que Itsuki sea lo suficientemente flexible para scripting rápido, pero lo suficientemente robusto para aplicaciones complejas que requieren alto rendimiento y control de memoria.
