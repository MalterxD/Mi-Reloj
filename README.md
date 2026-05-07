# CLI Clock

<p align="center">
  <img src="assets/Preview.png" width="600">
</p>

A digital clock for the terminal written in C.

## Installation
```bash
make
sudo make install
make setup
```

## Usage and Arguments

Run the command from anywhere:

```bash
    clock: Default format.

    clock -12: 12-hour mode.

    clock -b, --battery: Enable battery status display

    clock --help: Help. (obviously)
```
## Architecture

The code is designed following modularity principles:

    clock.h / clock.c: Rendering logic, terminal management (Raw Mode), and UI engine.
    battery.h / battery.c: System abstraction to read power supply status from /sys/class/power_supply/.
    
## Controls
```bash
    s: Toggle seconds.
    t: Toggle 12h/24h format.
    q or Esc: Exit the program.
    b: Toggle battery status display
```

## Configuration
Edit the ~/.clockrc file to customize color and behavior:
```bash
color=cyan
autocolor=true
format=24h
```
## Distro-based Colors

If autocolor=true is set and no specific color is forced, the clock detects your distribution and applies its signature color:

    Fedora: Blue.  
    Arch:   Cyan.  
    Ubuntu: Orange.  
    Debian: Red.  
    Mint:   Green.  

      Support for more distributions will be added over time.
## Available Colors:
    white, red, green, yellow, blue, magenta, cyan, orange, pink, gray.
