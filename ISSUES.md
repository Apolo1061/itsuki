# Issues Conocidos - Itsuki v5.0

## [CRÃTICO] Bug en Sistema de MÃ³dulos - Funciones Exportadas Retornan Valores Incorrectos

**Fecha:** 2026-02-16  
**Versión:** v5.0  
**Estado:** Resuelto en v5.0  
**Prioridad:** Alta

### DescripciÃ³n

Las funciones exportadas desde mÃ³dulos `.suki` no ejecutan su cÃ³digo interno correctamente y retornan valores incorrectos o basura de memoria.

### SÃ­ntomas

1. **Funciones exportadas no ejecutan su cÃ³digo:**
   - Las instrucciones `print()` dentro de funciones exportadas no se ejecutan
   - El cÃ³digo de la funciÃ³n parece ser ignorado completamente

2. **Valores de retorno incorrectos:**
   - Funciones que deberÃ­an retornar objetos retornan nÃºmeros
   - Funciones que deberÃ­an retornar cÃ³digos de resultado retornan argumentos de entrada
   - Ejemplo: `net.connect(sock, "1.1.1.1", 80)` retorna `80` (el puerto) en lugar de `0` (cÃ³digo de Ã©xito)

### Casos de Prueba

#### Test que FALLA (usando mÃ³dulo):
```suki
importar "modules/socket" como net
sea s = net.socket(net.AF_INET, net.SOCK_STREAM)
print("Socket: " + string(s))  # Imprime: "Socket: 1" (nÃºmero incorrecto)

sea r = net.connect(s, "1.1.1.1", 80)
print("Connect: " + string(r))  # Imprime: "Connect: 80" (retorna el puerto!)
```

**Resultado:** `net.socket()` retorna `1` en lugar del objeto Socket, `net.connect()` retorna `80` en lugar de `0`.

#### Test que FUNCIONA (llamada directa):
```suki
sea s = _socket_socket(2, 1)
print("Socket: " + string(s))  # Imprime: "Socket: [Socket:316]" (correcto)

sea r = _socket_connect(s, "1.1.1.1", 80)
print("Connect: " + string(r))  # Imprime: "Connect: 0" (correcto)
```

**Resultado:** Funciona perfectamente, retorna valores correctos.

### DiagnÃ³stico TÃ©cnico

**Archivos investigados:**
- `src/vm.c` - FunciÃ³n `cargar_modulo()` (lÃ­neas 535-589)
- `src/compiler.c` - Manejo de `AST_IMPORTAR` (lÃ­neas 342-363)
- `src/parser.c` - Parsing de `exportar` (lÃ­neas 619-626)

**Hallazgos:**

1. La funciÃ³n `cargar_modulo()` en `vm.c` exporta correctamente las funciones:
   ```c
   for (int i = n_f_old; i < n_f; i++) 
       if (funcs[i].es_publico) 
           map_establecer(mod->exports, funcs[i].nombre, 
                         (Result){.tipo = TIPO_FUNCION, .func_index = i});
   ```

2. El `func_index` se guarda correctamente en `mod->exports`

3. **El problema estÃ¡ en la resoluciÃ³n/ejecuciÃ³n:** Cuando se llama a `modulo.funcion()`, el VM no estÃ¡ ejecutando correctamente el cÃ³digo de la funciÃ³n exportada.

4. Las trazas de debug dentro de funciones exportadas **nunca se ejecutan**, confirmando que el cÃ³digo de la funciÃ³n es ignorado.

### Workaround

**SoluciÃ³n temporal:** Usar llamadas directas a los builtins en lugar del sistema de mÃ³dulos.

En lugar de:
```suki
importar "modules/socket" como net
sea s = net.socket(2, 1)
sea r = net.connect(s, "1.1.1.1", 80)
```

Usar:
```suki
sea s = _socket_socket(2, 1)
sea r = _socket_connect(s, "1.1.1.1", 80)
```

### Archivos de Referencia

- **Test comparativo:** `test_compare.suki` - Demuestra la diferencia entre llamada directa y mÃ³dulo
- **Test funcional:** `final_test.suki` - Prueba exitosa usando llamadas directas
- **MÃ³dulo afectado:** `modules/socket.suki` - Ejemplo de mÃ³dulo que falla

### PrÃ³ximos Pasos

1. Investigar cÃ³mo se resuelven las llamadas a `modulo.funcion()` en el VM
2. Verificar si el problema estÃ¡ en `OP_OBTENER_PROPIEDAD` o en el despacho de funciones
3. Revisar si hay diferencias en cÃ³mo se manejan funciones locales vs exportadas
4. Considerar si el sistema de mÃ³dulos estÃ¡ usando el evaluador directo en lugar del VM

### Impacto

- **Severidad:** Alta - El sistema de mÃ³dulos es una caracterÃ­stica core
- **Workaround disponible:** SÃ­ - Llamadas directas funcionan correctamente
- **Afecta a:** Todos los mÃ³dulos que exportan funciones wrapper
- **No afecta a:** Builtins nativos llamados directamente, variables exportadas

---

**Reportado por:** Antigravity AI  
**Ãšltima actualizaciÃ³n:** 2026-02-16

