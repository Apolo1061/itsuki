# **Funciones**

## Declaración
Las funciones se definen con la palabra clave `funcion`.

```python
funcion saludar(nombre) {
    printf("Hola " + nombre)
}
```

## Parametros y Argumentos
Itsuki soporta hasta 8 parámetros por funcion, los argumentos se pasan por valor.

```python
funcion sumar(a, b) {
    sea total = a + b
    printf("Suma: " + total)
}

sumar(10, 5) # Salida: Suma: 15
```

## Ámbito Local (Local Scope)
Todas las variables creadas dentro de una función (incluyendo los parametros) son **locales**.
- No afectan a las variables globales con el mismo nombre.
- Se destruyen al terminar la ejecución de la función.

```python
sea x = 100

funcion prueba() {
    sea x = 20
    printf(x) # Imprime 20 (Local)
}

prueba()
printf(x) # Imprime 100 (Global)
```
