# vivid simulator

## prerequisites
- install [Bun](https://bun.sh)
- install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html#installation-using-unofficial-packages)

## setup
```bash
cd web-sim2
bun install
```

## development

### rebuild after animation changes

if you make changes to `animations.h` or `anim_schema.h`, rebuild with:

```bash
bun run build-wasm
```

### run the development server

```bash
bun run dev
```
