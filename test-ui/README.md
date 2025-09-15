# test-ui

Desktop harness to extract the embedded web UI HTML from `web_ui.h` and serve it locally for rapid iteration.

## Build (CMake example)
```
mkdir -p build && cd build
cmake .. && cmake --build .
./test_ui
```
Then open http://localhost:8080

## What it does
- Recreates minimal Arduino macros and PROGMEM to compile `web_ui.h` as plain C++.
- Concatenates `INDEX_HTML_PREFIX`, `ANIM_SCHEMA_JSON`, `INDEX_HTML_SUFFIX` into one HTML string.
- Serves it with a tiny HTTP server providing stub endpoints:
  - `GET /api/state` returns a synthetic example state
  - `POST /api/cfg2` echoes `{"ok":true}`

## Why
Lets you tweak UI JS/CSS locally without reflashing the device.

## Notes
- This is a minimal harness; not all Arduino specifics are emulated.
- Update include paths if repository layout changes.
