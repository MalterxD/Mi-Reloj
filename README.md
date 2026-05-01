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
    v: Entrar al modo Temporizador / Volver al Reloj.
    Espacio: Pausar o reanudar el temporizador (solo en modo temporizador).
    q o Esc: Salir del programa.
```
## Configuracion

Edita el archivo ~/.relojrc para personalizar el color y formato:
```bash
color=cyan
format=12h
```
## Colores por Distro

Si no configuras nada en tu ~/.relojrc, el reloj detecta qué distro usas y se pone de su color:

    Fedora: Azul.  
    Arch: Cyan.  
    Ubuntu: Naranja.  
    Debian: Rojo.  
    Mint: Verde.  

    Iré añadiendo más colores para otras distros poco a poco, según vaya teniendo tiempo.
## Colores disponibles:
    white, red, green, yellow, blue, magenta, cyan, orange, pink, gray.
