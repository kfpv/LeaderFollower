# Build WebAssembly from animations.h via a small shim
# Requires emscripten (emcc) active in your shell

TARGET = sim
SRC = wasm_shim.cpp
OUT_DIR = dist

EMCC ?= emcc
CXXFLAGS = -O3 -s MODULARIZE=1 -s ENVIRONMENT=web -fno-exceptions -fno-rtti -I.
LDFLAGS = -s ALLOW_MEMORY_GROWTH=1 -s EXPORT_ES6=1 \
	-s EXPORTED_FUNCTIONS='["_anim_branches","_anim_leds_per_branch","_anim_total_leds","_anim_eval","_anim_eval_store","_anim_eval2","_anim_get_value","_anim_value_at"]'

all: $(OUT_DIR)/$(TARGET).mjs $(OUT_DIR)/$(TARGET).wasm

$(OUT_DIR)/$(TARGET).mjs: $(SRC) ../animations.h | $(OUT_DIR)
	$(EMCC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRC)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
