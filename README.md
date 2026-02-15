<div align="center">

# The Itsuki programming language

![Logo](icons/itsuki_logo.png)

### ***Lenguaje moderno que hace que programar a bajo nivel sea facil***

<p align="center">
  <strong>Rapido</strong> • <strong>Seguro</strong> • <strong>Expresivo</strong> • <strong>Productivo</strong>
</p>

[![License](https://img.shields.io/badge/license-MIT%20OR%20Apache--2.0-blue.svg?style=for-the-badge)](LICENSE)
![Version](https://img.shields.io/badge/version-4.5.3-green.svg?style=for-the-badge)
[![Discord](https://img.shields.io/discord/Xw2eaWvNsz?logo=discord&style=for-the-badge)](https://discord.gg/Xw2eaWvNsz)
[![Stars](https://img.shields.io/github/stars/Apolo1061/itsuki?style=for-the-badge&logo=github)](https://github.com/Apolo1061/itsuki/stargazers)

<p align="center">
  <a href="https://docs.itsuki.org">📚 Documentación</a> •
  <a href="https://play.itsuki.org">🎮 Playground</a> •
  <a href="https://discord.gg/Xw2eaWvNsz">💬 Comunidad</a>
</p>

</div>

<br/>

---

<br/>

<div align="center">

## **¿Por que utilizar Itsuki?**

</div>

<table>
<tr>
<td width="50%">

### **Por:**

```
 Lenguaje cerca de C/C++
 Puede utilizar librerias de C/C++
 Zero-cost abstractions
 Benchmarks comprobados
```

Velocidad comparable a C/C++ con optimizaciones en tiempo de compilación. Ideal para aplicaciones de alto rendimiento, sistemas embebidos.

</td>

<tr>
<td width="50%">

## **Productividad Máxima**

```
 Sintaxis clara e intuitiva
 Herramientas de primera
 Gestor de paquetes integrado
 Depurador avanzado
```

Sintaxis clara y expresiva, documentación exhaustiva, herramientas de desarrollo de primera clase y un ecosistema de paquetes en crecimiento.

</td>
</tr>
</table>

<br/>

---

<br/>

<div align="center">

## Instalacion

### *Se puede instalar de de forma rapida*

</div>

<br/>

<table>
<tr>
<th>🐧 Linux  /  🍎 macOS</th>
</tr>
<tr>
<td>

```bash
git clone github.com/Apolo1061/itsuki.git
```

### *Entras a la carpeta y ejecutas el siguiente comando*

```bash
make install
```

</td>
</tr>
</table>
<br/>

## Estructura del Proyecto

### *Organizacion clara y modular*

</div>

<br/>

```
📂itsuki/
│
├── 📂 modules/                # Archivo de los modulos
│
├── 📂 obj/                    # Archivo de obj
│
├── 📂 docs/                   # Sistema de construcción
│   ├── 📂 languaje/           # Carpeta sobre el lenguaje
│   │   ├── 📄 builtins.md     # Funciones del lenguaje
│   │   ├── 📄 funciones.md    # Guia para hacer funciones
│   │   ├── 📄 intro.md        # Resumen de la arquitectura
│   │   ├── 📄 STYLE_GUIDE.md  # Guia de estilos
│   │   └── 📄 sintaxis.md     # Guia para programar itsuki
│   │
│   └── 📄 manual.md           # Manual simple del lenguaje
│
├── 📂 src/                    # No explico todo porque tardaria mucho
│
├── 📄 Makefile                # Archivo Makefile
├── 📄 README.md               # Este archivo
├── 📄 RELEASES.md             # Guia de versiones
├── 📄 CODE_OF_CONDUCT.md      # Codigo de conducta
├── 📝 CONTRIBUTING.md         # Guia de contribucion
├── 📜 LICENSE-MIT             # Licencia MIT
└── 📜 LICENSE-APACHE          # Licencia Apache 2.0
```

<br/>

<div align="center">

## Como contribuir a Itsuki

### *Unete a nuestra comunidad de desarrolladores atravez de discord*

</div>

<br/>

<p align="center">
  <img src="https://contrib.rocks/image?repo=Apolo1061/itsuki" />
</p>

<p align="center">
  <i>Gracias a todos nuestros contribuidores 🎉</i>
</p>

<br/>

<div align="center">

### 💡 Hay muchas formas de contribuir

</div>

<table>
<tr>
<td align="center" width="33%">

### 🐛 Reportar Bugs

¿Encontraste un problema?

[**Abrir Issue**](https://github.com/Apolo1061/itsuki/issues/new?template=bug_report.md)

Ayudanos a mejorar reportando bugs con detalles claros

</td>
<td align="center" width="33%">

### 💡 Sugiere tus ideas

Tienes una buena idea

[**Proponer Feature**](https://github.com/Apolo1061/itsuki/issues/new?template=feature_request.md)

Comparte tus ideas para nuevas características

</td>

<tr>
<td align="center" width="33%">

### 🎨 Diseñar

¿Eres diseñador?

[**Design Issues**](https://discord.gg/JB6Aa28Q6zn)

Ayuda con logos y material visual comunicate conmigo a mi DC 

</td>

</tr>
</table>

<br/>

<div align="center">

### 🔄 Proceso de Contribución

</div>

```mermaid
graph LR
    A[Fork] --> B[Branch]
    B --> C[Code]
    C --> D[Test]
    D --> E[Commit]
    E --> F[Push]
    F --> G[Pull Request]
    G --> H[Review]
    H --> I[Merge]
```

<br/>

<table>
<tr>
<td>

#### 1 Fork y Clone
```bash
# Fork en GitHub, luego:
git clone https://github.com/Apolo1061/itsuki.git
cd itsuki
```

</td>
<td>

#### 2 Crea una Rama
```bash
git checkout -b feature/Thebest-feature
# o
git checkout -b fix/bug-importante
```

</td>
</tr>
<tr>
<td>

</td>
<td>

#### 3 Prueba Todo
```bash
# Asegurate que todo funciona
make test
```

</td>
</tr>
<tr>
<td>

#### 5 Commit
```bash
git add .
git commit -m "feat: añade característica X"
# Usa conventional commits
```

</td>
<td>

#### 6 Abre un PR
```bash
git push origin feature/Thebest-feature
# Luego abre el PR en GitHub
```

</td>
</tr>
</table>

<br/>

<div align="center">

### 📋 Antes de Contribuir

</div>

<table>
<tr>
<td width="50%">

**📖 Lee nuestras guias**
- [Guia de Contribucion](CONTRIBUTING.md)
- [Codigo de Conducta](CODE_OF_CONDUCT.md)
- [Guía de Estilo](docs/STYLE_GUIDE.md)

</td>
</tr>
</table>

<br/>

<div align="center">

### 🎨 Estándares de Código

</div>

```bash
# Formatear código automáticamente
itsuki fmt

# Verificar estilo
itsuki lint

# Ejecutar todos los checks
make test
```

<br/>

<div align="center">

### 🏆 Reconocimiento

Todos los contribuidores son reconocidos en:
- 📜 [Archivo de Contribuidores](CONTRIBUTORS.md)
- 🌟 [Notas de Release](RELEASES.md)

</div>

<br/>

---

<br/>

<div align="center">

##  Testing

### *Calidad asegurada en cada línea de código* ✅

</div>

<br/>

<table>
<tr>
<td width="50%">

###  Ejecutar Todas las Pruebas

```bash
make test
```

</td>

## Roadmap

</div>

<br/>

```mermaid
gantt
    Plan de mejora para itsuki
    dateFormat:  YYYY-MM-DD
    section v4.5
    Sistema de modulos mejorado:      2026-02-18,
    Mejor mensaje de errores:         2026-02-18,
    section v4.6
    Optimizaciones del transpilador:  2026-02-27,
    Mejora del recolector de basura:  2026-03-25,
    section v4.7
    Async/Await nativo            :2026-04-29,
    Mejoras de performance        :2026-05-05,
```

<div align="center">

## ***Estado actual del Itsuki languaje***

### *Transparencia total sobre nuestro progreso*

</div>

<br/>

<table>
<tr>
<td width="50%">

### Partes princiaples

| Partes | Estado | Estabilidad |
|------------|--------|-------------|
|  Transpilador | ⚙️Beta | ![](https://img.shields.io/badge/stability-beta-yellow?style=flat-square) |
|  Compilador Base | 🚧 En desarrollo | ![](https://img.shields.io/badge/stability-desarrollo-red?style=flat-square) |
|  Libreria Estandar | 🚧 En desarrollo | ![](https://img.shields.io/badge/stability-desarrollo-red?style=flat-square) |
|  Sistema de Tipos | ⚙️Beta | ![](https://img.shields.io/badge/stability-beta-yellow?style=flat-square) |
|  Async/Await | 🚧 En desarrollo | ![](https://img.shields.io/badge/stability-desarrollo-red?style=flat-square) |
|  WebAssembly | 🚧 En desarrollo | ![](https://img.shields.io/badge/stability-desarrollo-red?style=flat-square) |
|  LSP Server | ⚙️Beta | ![](https://img.shields.io/badge/stability-beta-yellow?style=flat-square) |
|  Documentacion | ⚙️Beta | ![](https://img.shields.io/badge/stability-beta-yellow?style=flat-square) |

</td>
<td width="50%">

### Herramientas y Soporte

| Herramienta | Soporte | 
|-------------|---------|
| VS Code | 🚧 En desarrollo |
| IntelliJ IDEA | 🚧 En desarrollo |
| Vim/Neovim | 🚧 En desarrollo |
| Emacs | 🚧 En desarrollo |
| Sublime Text | 🚧 En desarrollo |
| Atom | 🚧 En desarrollo | 
| Debugger | 🚧 En desarrollo | 
| Profiler | 🚧 En desarrollo | 
| Package Manager | 🚧 En desarrollo | 

</td>
</tr>
</table>

<div align="center">

## 🌟 Star History

<a href="https://star-history.com/#Apolo1061/itsuki&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Apolo1061/itsuki&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Apolo1061/itsuki&type=Date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Apolo1061/itsuki&type=Date" width="600"/>
  </picture>
</a>

</div>


<div align="center">

### ⭐ Si te gusta Itsuki, ¡danos una estrella! ⭐

[![Star on GitHub](https://img.shields.io/github/stars/Apolo1061/itsuki.svg?style=social)](https://github.com/Apolo1061/itsuki/stargazers)

</div>
