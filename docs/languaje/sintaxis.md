# Guía de Sintaxis 

## Variables
En Itsuki, las variables se definen con la palabra clave `sea`. El lenguaje tiene tipado dinamico y estatico vamos primero con el dinamico

### **Tipado dinamico**

```python
sea nombre = "Itsuki"
sea version = 5.0
sea edad = 18
```

### **Estatico**

```python
sea nombre:string = "Itsuki"
sea version:float = 5.0
sea edad:int = 18
```

Itsuki tiene los mismos especificadors de tipo que C

## **Preprocesador y macros**

itsuki al igual que se tiene un sistema de macros las cuales son

constantes inmutables por convencion en el tiepo de ejecucion

Itsuki tiene las mismas macros que C, para definir una macro su estructura es

**%macro <tipo> <Name de la macro> <Accion>**

### **Sintaxis de macros**
```python
%macro definir PI 3.14159
```

### **Otro ejemplo**

```python
%macro definir DEBUG
%macro definir VERSION "1.0.0"

%ifdef DEBUG
    print("Ejecutando en modo DEBUG...")
%endif

%ifndef PRODUCCION
    print("Servidor de pruebas")
%endif
```

## Operadores
### Matemáticos
- `+` : Suma (y concatenación de texto)
- `-` : Resta
- `*` : Multiplicación
- `/` : División
- `++` : Incrementar el numero 
- `++` : Decrementar el numero

### Concatenación
```python
sea msg = "Versión: " + 3.8
print(msg) # Imprime "Versión: 3.8"
```

## Comentarios
Usa el símbolo `#` para comentarios de una sola línea.
```python
# Esto es un comentario que Itsuki ignora
```

## f-strings
las f-strings de itsuki permite incrustar expresiones directamente dentro de las cadenas

```python
sea name = "Pedro"
sea edad = 18
print(f"{name} tiene {edad} años")
print(f"El siguiente año {name} tendra {edad + 1} años")
```

## Arrays de forma dinamica

```python
sea frutas = ["mazana", "pera", "kiwi"]
agregar(frutas, "mango") # Funcion integrada de Itsuki
sea count_fruit = largo(frutas) # retorna 4 (largo() funcion integrada de itsuki)
sea ultima = frutas[3] # Acceso por indice
```

## Arrays Manuales



## Tablas hash / Diccionarios

```python
sea db = {
    "uid": 1,
    "user": "Apolo_dev"
    "rol": ["Editor", "Admin", "Creador"]
}
db

db["ultima_conexion"] = "2024-02-12"
print(db["user"])
```

## Enums
Los Enums en Itsuki pueden contener datos asociados, similares a los de Rust.

```python
enum Estado {
    Cargando,
    Error(mensaje),
    Exito(datos)
}

sea resultado = Estado.Error("Conexion perdida")
```

## Condicionales y `es` (Type Matching)

```python
sea valor = [1, 2]

si(valor es array){
    print("Es una lista")
}sino si(valor es string){
    print("Es una cadena")
}
```

## Bucles

```python
# Bucle para con paso
para (i de 0 a 100 paso 10) {
    print(f"Progreso: {i}%")
}

# Iteracion sobre colecciones
sea items = [10, 20, 30]
para (x en items) {
    print(x)
}
```

## Funciones de Primera Clase

```python
funcion suma(a, b) {
    retornar a + b
}

sea operacion = suma
print(operacion(5, 5)) # Imprime 10
```

## Lambdas (Funciones Anonimas)
```python
sea cuadrado = lambda(x){
    retornar x * x 
}

sea lista = [1, 2, 3]
# Suponiendo una funcion map implementada o integrada
sea mapeados = map(lista, lambda(n) { retornar n * 2 })
```

## Clases y Herencia

```python
clase Entidad {
    sea x = 0
    sea y = 0
    
    funcion mover(dx, dy) {
        este.x = este.x + dx
        este.y = este.y + dy
    }
}

clase Jugador hereda Entidad {
    sea nombre
    
    funcion inicializar(n) {
        este.nombre = n
    }
    
    funcion presentarse() {
        print(f"Soy {este.nombre} en posicion {este.x}, {este.y}")
    }
}

sea p1 = nueva Jugador()
p1.inicializar("Itsuki_Master")
p1.mover(10, -5)
p1.presentarse()
```

## Importacion de modulos

```python
# Importar todo el modulo
importar "matematica.suki" como mat

# Importar elementos especificos
desde "red.suki" importar conectar, enviar
```

## C Extern (Interoperabilidad con C)

Itsuki puede llamar directamente a funciones escritas en C.
```python
c_extern funcion system(comando: string): int32
system("ls -la")
```

## Gestion de Errores y Excepciones

```python
intentar{
    sea data = leer_archivo("config.json") # Funcion de itsuki
    si(largo(data) == 0){
        lanzar "Archivo vacio"
    }
} capturar(error){
    print(f"Fallo critico: {error}")
} finalmente{
    print("Cerrando descriptores...")
}
```

# PROXIMAMENTE
