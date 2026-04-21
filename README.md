# Reloj CLI

Reloj digital para la terminal escrito en **C**.

## Instalacion
```bash
make
sudo make install
make setup
```

## Uso y Argumentos

Ejecuta el comando desde cualquier lugar:

```bash
    clock (Formato por defecto)

    clock -12 (Modo 12 horas)

    clock --help (Ayuda)
```
## Controles
```bash
    s: Alternar tamaño de segundos.

    t: Alternar 12h/24h.

    q: Salir.
```
## Configuracion

Edita el archivo ~/.relojrc para personalizar el color y formato:
```bash
color=cyan
format=12h
```
## Colores disponibles:

white, red, green, yellow, blue, magenta, cyan, orange, pink, gray.