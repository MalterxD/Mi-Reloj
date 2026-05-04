# CLI Clock

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

    clock --help: Help. (obviously)
```
## Architecture

The code is designed following modularity principles:

    clock.h: Definition of state structures and function contracts.
    clock.c: Rendering logic, terminal managment (Raw Mode), and timer engine.

## Controls
```bash
    s: Toggle seconds.
    t: Toggle 12h/24h format.
    Space: Pause or resume the timer (Timer mode only).
    q or Esc: Exit the program.
```
## Configuration

Edit the ~/.clockrc file to customize color and format:
```bash
color=cyan
autocolor=true
format=24h
```
## Distro-based Colors

if no configuration is found in ~/.clockrc, the clock detects your distribution and applies its signature color:

    Fedora: Azul:.  
    Arch: Cyan.  
    Ubuntu: Orange.  
    Debian: Red.  
    Mint: Green.  

      Support for more distributions will be added over time.
## Available Colors:
    white, red, green, yellow, blue, magenta, cyan, orange, pink, gray.
