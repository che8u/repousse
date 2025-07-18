CXX        := clang++
DEV_CXXFLAGS := -std=c++23 -fobjc-arc -O0 -g -fno-omit-frame-pointer \
                -I/opt/homebrew/include
PROD_CXXFLAGS := -std=c++23 -fobjc-arc -O3 -DNDEBUG -flto -ffast-math -I/opt/homebrew/include

LDFLAGS       := -framework Foundation \
                 -framework Metal \
                 -L/opt/homebrew/lib -flto
LIBS       := -lbenchmark -lbenchmark_main -lpthread

SRC        := main.cc
METAL_SRC  := gol_buffer.metal    gol_texture.metal
METAL_AIR  := gol_buffer.air      gol_texture.air
METAL_LIB  := gol_buffer.metallib gol_texture.metallib
OUT        := bin

FRAME_DIRS := gol_frames_buffer gol_frames_texture
SYMBOL_DIR := bin.dSYM/
CSV_FILES  := *.csv

# Default target
.DEFAULT_GOAL := dev

.PHONY: all dev prod run clean rebuild

rebuild: clean all

# Default dev build
dev: CXXFLAGS := $(DEV_CXXFLAGS)
dev: all

# Production build
prod: CXXFLAGS := $(PROD_CXXFLAGS)
prod: STRIP := strip
prod: all
	@echo "=== Stripping debug symbols from binary ==="
	$(STRIP) $(OUT)
	@echo "✓ Production binary ready"

all: $(METAL_LIB) $(OUT)
	@echo "=== Build completed successfully ==="

# Metal compilation steps
# gol_buffer.air: gol_buffer.metal
# 	@echo "=== Compiling Metal shader: gol_buffer.metal → gol_buffer.air ==="
# 	$(Q)xcrun -sdk macosx metal -c gol_buffer.metal -o gol_buffer.air
# 	@echo "✓ Metal shader compiled successfully"

# gol_texture.air: gol_texture.metal
# 	@echo "=== Compiling Metal shader: gol_texture.metal → gol_texture.air ==="
# 	$(Q)xcrun -sdk macosx metal -c gol_texture.metal -o gol_texture.air
# 	@echo "✓ Metal shader compiled successfully"

gol_buffer.air: gol_buffer.metal
	@echo "=== Compiling Metal shader: gol_buffer.metal → gol_buffer.air ==="
	$(Q)xcrun -sdk macosx metal -c -O3 gol_buffer.metal -o gol_buffer.air
	@echo "✓ Metal shader compiled successfully"

gol_texture.air: gol_texture.metal
	@echo "=== Compiling Metal shader: gol_texture.metal → gol_texture.air ==="
	$(Q)xcrun -sdk macosx metal -c -O3 gol_texture.metal -o gol_texture.air
	@echo "✓ Metal shader compiled successfully"

gol_buffer.metallib: gol_buffer.air
	@echo "=== Linking Metal library: gol_buffer.air → gol_buffer.metallib ==="
	$(Q)xcrun -sdk macosx metallib gol_buffer.air -o gol_buffer.metallib
	@echo "✓ Metal library linked successfully"

gol_texture.metallib: gol_texture.air
	@echo "=== Linking Metal library: gol_texture.air → gol_texture.metallib ==="
	$(Q)xcrun -sdk macosx metallib gol_texture.air -o gol_texture.metallib
	@echo "✓ Metal library linked successfully"

# Shared build rule for both dev and prod
$(OUT): $(SRC) $(METAL_LIB)
	@echo "=== Building C++ executable: $@ ==="
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)
	@echo "✓ C++ executable built successfully"

run: all
	@echo "=== Running executable with Metal debug layer enabled ==="
	@MTL_DEBUG_LAYER=1 ./$(OUT)

clean:
	@echo "=== Cleaning build artifacts ==="
	@echo "Removing: $(OUT) $(METAL_AIR) $(METAL_LIB) $(CSV_FILES)"
	rm -f $(OUT) $(METAL_AIR) $(METAL_LIB) $(CSV_FILES)
	rm -fr $(SYMBOL_DIR) $(FRAME_DIRS)
	@echo "✓ Clean completed"
