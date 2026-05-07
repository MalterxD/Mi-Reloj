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
To uninstall:

```bash
sudo make uninstall
```

## Usage and Arguments

Run the command from anywhere:

```bash
clock              # Default (24h format)
clock -12          # Start in 12-hour format
clock -b           # Enable battery display
clock -s           # Enable large seconds
clock -C cyan      # Set digit color by name
clock -C 39        # Set digit color by ANSI 256 code
clock --help       # Show help
```

Flags `-b` and `-s` also accept `on` / `off`:

```bash
clock -b off       # Force battery off even if enabled in config
clock -s on        # Same as -s
```

## Architecture

The code is designed following modularity principles:

    clock.h / clock.c: Rendering logic, terminal management (Raw Mode), and UI engine.
    battery.h / battery.c: System abstraction to read power supply status from /sys/class/power_supply/.
    
## Controls

| Key       | Action                  |
|-----------|-------------------------|
| `s`       | Toggle seconds display  |
| `t`       | Toggle 12h / 24h format |
| `b`       | Toggle battery display  |
| `q` / Esc | Exit                    |

## Configuration
Edit the ~/.clockrc file to customize color and behavior:

```ini
# ~/.clockrc

color=cyan          # Digit color (name or 0-255)
autocolor=true      # Auto-detect color based on distro
format=24h          # 24h or 12h
show_battery=false  # Show battery status
seconds=false       # Show large seconds
```

## Distro-based Colors

When `autocolor=true` and no `-C` flag is passed, the clock picks a color based on your distro:

| Distro       | Color  |
|--------------|--------|
| Fedora       | Blue   |
| Arch Linux   | Cyan   |
| Ubuntu       | Orange |
| Debian       | Red    |
| Linux Mint   | Green  |
| Kali Linux   | Blue   |
| Pop!\_OS     | Cyan   |
| Manjaro      | Green  |
| Void Linux   | Olive  |
| openSUSE     | Green  |
  
      Support for more distributions will be added over time.
## Available Colors:
  
`red`, `green`, `blue`, `yellow`, `magenta`, `cyan`, `orange`, `white`

Or use any ANSI 256 color code directly: `color=214`
