# Reloj Digital

Reloj digital para la terminal escrito en **C**.

## Instalacion
```bash
make
sudo make install
make setup

## Uso y argumentos
Ejecuta el comando desde cualquier lugar:
   clock (Formato por defecto)
   clock -12 (Modo 12 horas)
   clock --help (Ayuda)

## Controles
s: Alternar tamaño de segundos.
t: Alternar 12h/24h.
q: Salir.

## Configuración
Edita el archivo `~/.relojrc` para personalizar el color y formato:

```ini
color=cyan
format=12h

## Colores disponibles:
white, red, green, yellow, blue, magenta, cyan, orange, pink, gray.