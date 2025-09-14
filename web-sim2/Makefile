# Build WebAssembly from animations.h via a small shim
# Requires emscripten (emcc) active in your shell

TARGET = sim
SRC = wasm_shim.cpp
OUT_DIR = dist

EMCC ?= emcc
CXXFLAGS = -O3 -s MODULARIZE=1 -s ENVIRONMENT=web -fno-exceptions -fno-rtti -I.
LDFLAGS = -s ALLOW_MEMORY_GROWTH=1 -s EXPORT_ES6=1 \
	-s EXPORTED_FUNCTIONS='["_anim_branches","_anim_leds_per_branch","_anim_total_leds","_anim_count","_anim_name","_anim_param_count","_anim_param_id","_param_info","_param_set","_param_get","_anim_eval_global","_anim_eval_store_global","_anim_eval2_global","_anim_eval","_anim_eval_store","_anim_eval2","_anim_get_value","_anim_value_at","_anim_name_safe","_param_name_safe","_malloc","_free","_read_uint8","_read_int8","_read_uint16","_read_int16","_read_uint32","_read_int32","_read_float","_read_double"]'

all: $(OUT_DIR)/$(TARGET).mjs $(OUT_DIR)/$(TARGET).wasm

$(OUT_DIR)/$(TARGET).mjs: $(SRC) ../animations.h ../anim_schema.h | $(OUT_DIR)
	$(EMCC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRC)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
