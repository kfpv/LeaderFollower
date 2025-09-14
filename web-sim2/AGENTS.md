when trying to build the web-sim, please use cd web-sim2 && bun run <command>

please use bun not npm.

do not read files in dist (these are the built/modified files), instead these are the built wasm_ship.cpp

keep in mind you have context7 get library docs tool - use it when dealing with libraries.

run "cd web-sim2 && bun run build-wasm && bun run build" to test if it builds 

DO NOT RUN bun run dev - the dev server is already running in background. 

Ignore error "In included file: 'Arduino.h' file not found"