# RELEASES

## [v5.0] - 2026-02-17
### Añadido
- **Módulos nativos C**: soporte para cargar extensiones dinamicas con `importar "modules/xxx"` usando `.dll/.so/.dylib`.
- **Módulo `math` nativo**: nuevo modulo de matematicas en C para operaciones numericas y estadísticas.
- **Guía de módulos nativos**: nuevo documento `modulos_dm.md` con pasos para Windows y Linux.

### Docs
- **Web de documentación**: añadida sección dedicada a módulos nativos C con instrucciones de compilación por plataforma.

## [v4.5] - 2026-02-16
### Fix
- **Sockets**: Corregido error de red.

## [v4.5] - 2026-02-15
### Añadido
- **Subcomando `itsuki fmt`**: Formateador de codigo nativo con indentacion de 4 espacios.
- **Subcomando `itsuki lint`**: Analisis estatico de codigo para detectar variables no usadas y codigo inalcanzable.
- **Mejora en `itsuki-cy update all`**: Ahora permite actualizar todos los paquetes instalados de forma masiva.
- **Instalacion Inteligente**: El Makefile ahora detecta y elimina versiones previas antes de instalar en Windows y Linux.

### Cambios
- Version global del motor actualizada a v4.5.
- Mejoras en la ayuda del transpilador y el REPL.

## [v4.4] - 2026-02-10
### Añadido
- Soporte completo para **Sockets (TCP/UDP)** en Windows y Linux.
- Integración de `itsuki-cy` (Gestor de paquetes).
- Renombrado de funciones built-in a español (`escribir`, `leer`, `input`, etc.).
- Compilacion nativa optimizada con `bundle.c`.

## [v4.0]
- Implementacion de la VM basada en Bytecode.
- Soporte para Programacion Orientada a Objetos (Beta).
- Recolector de Basura (Mark & Sweep).
