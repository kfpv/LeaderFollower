Web simulation for animations.h

This folder builds a small site that compiles the existing C++ `animations.h` to WebAssembly via Emscripten and visualizes a snowflake-like layout (4 branches × 7 LEDs each). Colors fade from black to purple according to the animation output values.

What's included
- wasm/ binding: tiny C++ shim that exposes functions to JS without modifying `animations.h`.
- Static site: `index.html`, `style.css`, `sim.js`.
- Build scripts for Emscripten.

Prereqs
- Install Emscripten (emsdk). See: https://emscripten.org/docs/getting_started/downloads.html
- Activate it in this shell before building.

Quick start
1) Build the WASM module:
   - macOS/zsh
     source ~/emsdk/emsdk_env.sh
     make -C web-sim

2) Serve the folder (any static server). For example with Python:
     cd web-sim
     python3 -m http.server 8000

3) Open http://localhost:8000

Notes
- We copy the existing `../animations.h` unchanged; the shim includes it directly with `#include "../animations.h"`.
- You can switch animation types with the UI. Branch mode toggle follows the header logic.
