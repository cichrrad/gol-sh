
# Terminal Game of Life Screensaver

A fun little project: Conway’s **Game of Life** implemented as a colorful ASCII screensaver for your terminal.  
It continuously evolves a universe of cells, repopulating when the world dies out or becomes too stable/boring.  
Features include:

- Adjustable grid size (auto-detects your terminal by default).
- Configurable simulation speed and initial coverage.
- Stable-cell detection: automatically restarts when the universe becomes too static.
- Color options for foreground and background — including a rotating **rainbow mode** 🌈.
- Customizable cell glyph and box-drawing borders.

## Building

### Using g++
```bash
g++ -std=c++20 -O2 -o gof main.cpp
```

### Using CMake

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./gof [OPTIONS]
```

### Options

* `--window-width N` / `--window-height N` : set grid dimensions (otherwise autodetected).
* `--init-coverage PERCENT` : percentage of cells alive at start (default: 10).
* `--speed MS` : delay between frames (default: 250, min: 10).
* `--color-fg COLOR` : foreground color.
* `--color-bg COLOR` : background color.
* `--tile-cell "GLYPH"` : character used for cells (default: █).
* `--stable-treshold N` : % of long-lived cells that triggers restart.
* `--one-universe` : disable automatic repopulation.

Colors: `black, red, green, yellow, blue, magenta, cyan, white, default, rainbow`

## Notes

* Developed and tested on **Linux Mint** using
  **GNU bash, version 5.2.21(1)-release (x86\_64-pc-linux-gnu)**.
* Not tested on Windows, macOS, or other terminals.
  Compatibility with different shells/terminal emulators is not guaranteed.

## Preview

Run it in a terminal and enjoy a living, looping ASCII universe ✨

