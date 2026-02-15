# Guía de Contribución a Itsuki

Gracias por tu interes en contribuir a Itsuki te lo agradesco mucho bro. Esta guia resume el flujo de trabajo, las normas de estilo y las expectativas para cambios en el lenguaje y herramientas asociadas.

## Requisitos Previos
- Conocimientos básicos de programación en C y en el lenguaje Itsuki.
- Entorno de compilación C (GCC/Clang) y un editor/IDE.


## Cómo Empezar
- Revisa la [Guia de Estilo](./docs/STYLE_GUIDE.md) antes de enviar cambios.
- Abre un issue para reportar errores o proponer nuevas funciones.
- Para cambios sustanciales del lenguaje, discute primero el diseño en un issue.

## Flujo de Trabajo
1. Haz un fork del repositorio.
2. Crea una rama por tarea:  
   - `feat/nombre-corto`
   - `fix/bug-descriptivo`
   - `docs/tema`
   - `refactor/modulo`
3. Haz commits claros siguiendo Convenciones de Commits (ver abajo).
4. Abre un Pull Request (PR) hacia `main` con una descripción breve y precisa.

## Convenciones de Commits
- Usa Conventional Commits (en español o inglés), por ejemplo:
  - `feat(parser): soporte para enums`
  - `fix(lexer): corregir número de línea en tokens`
  - `docs: añadir guía de contribución`
  - `refactor(vm): extraer utilidades de pila`
  - `test: casos para try/catch`
- Mensajes cortos en la línea de asunto; detalla contexto en el cuerpo si es necesario.

## Estilo de Código
- Lenguaje Itsuki: sigue estrictamente [STYLE_GUIDE.md](./STYLE_GUIDE.md).
- C (motor de Itsuki):
  - Indentación con 4 espacios.
  - Nombres descriptivos y consistentes.
  - Evita macros complejas; prioriza funciones estáticas cuando sea posible.
  - Comentarios breves con `//` cuando aporten contexto los puedes poner en español y en ingles (proiridad a la español).

## Cambios en el Lenguaje
Si agregas o modificas sintaxis o built-ins, asegúrate de mantener la coherencia en todos los módulos relevantes:
- Léxico: `src/lexer.c` (nuevos tokens, palabras clave, literales).
- Árbol/Tipos: `src/headers/itsuki.h` (enums de tokens, nodos AST, tipos exactos si aplica).
- Parser/Compilador: `src/parser.c`, `src/compiler.c` (nuevos nodos y reglas).
- VM/Evaluador: `src/vm.c`, `src/evaluator.c` (semántica de ejecución o bytecode).
- Built-ins: `src/builtins.c` (implementación y registro).
- Playground: `web-ide/js/itsuki-mode.js` (resaltado de sintaxis) y `web-ide/js/main.js`/`server.js` si afecta la ejecución interactiva.

Incluye ejemplos mínimos que demuestren la nueva característica y añade casos negativos si es posible.

## Tests y Verificación
- Si existe un conjunto de tests, ejecútalos antes de abrir el PR.
- Agrega casos de prueba para nuevas sintaxis o built-ins.

## Documentación
- Actualiza ejemplos y la [Guía de Estilo](./STYLE_GUIDE.md) cuando cambien reglas de sintaxis o convenciones.
- Explica brevemente el porque del cambio en el PR y enlaza a issues relevantes.

## Política de PRs
- Un PR debe enfocarse en un objetivo claro y acotado.
- Separa refactors de cambios funcionales cuando sea posible.
- Resuelve conflictos con `main` antes de solicitar revisión.

## Seguridad y Calidad
- No incluyas credenciales, claves o tokens en el repositorio.
- Maneja errores con mensajes claros y en español cuando corresponda.
- Evita impresiones de depuración dejadas en el código final.

## Código de Conducta
- Se respetuoso, empatico y colaborativo.
- Las revisiones buscan mejorar la calidad del proyecto; evita respuestas defensivas.



¡Gracias por contribuir a Itsuki te lo agradesco mucho!

