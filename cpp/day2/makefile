CXX        := clang++
CXXFLAGS   := -std=c++23 -fobjc-arc -O3 -ffast-math \
               -I/opt/homebrew/include
LDFLAGS    := -framework Foundation \
               -framework Metal \
               -L/opt/homebrew/lib
LIBS       := -lbenchmark -lbenchmark_main -lpthread
# ASAN_FLAGS := -fsanitize=address -g -fno-omit-frame-pointer

SRC        := main.cc
METAL_SRC  := convolution.metal
METAL_AIR  := convolution.air
METAL_LIB  := convolution.metallib
OUT        := bin

CSV_FILES  := input_signal.csv mask.csv output_signal.csv

.PHONY: all run clean

all: $(METAL_LIB) $(OUT)
	@echo "=== Build completed successfully ==="

# 1) Compile .metal → .air
$(METAL_AIR): $(METAL_SRC)
	@echo "=== Compiling Metal shader: $< → $@ ==="
	xcode-select --switch /Applications/Xcode.app/Contents/Developer
	@echo "Command: xcrun -sdk macosx metal -c $< -o $@"
	xcrun -sdk macosx metal -c $< -o $@
	@echo "✓ Metal shader compiled successfully"

# 2) Link .air → .metallib
$(METAL_LIB): $(METAL_AIR)
	@echo "=== Linking Metal library: $< → $@ ==="
	@echo "Command: xcrun -sdk macosx metallib $< -o $@"
	xcrun -sdk macosx metallib $< -o $@
	@echo "✓ Metal library linked successfully"

# 3) Build the C++ executable (depends on the metallib being up-to-date)
$(OUT): $(SRC) $(METAL_LIB)
	@echo "=== Building C++ executable: $@ ==="
	@echo "Source files: $(SRC)"
	@echo "Metal library: $(METAL_LIB)"
	@echo "Command: $(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)"
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)
	@echo "✓ C++ executable built successfully"

run: all
	@echo "=== Running executable with Metal debug layer enabled ==="
	@echo "Command: MTL_DEBUG_LAYER=1 ./$(OUT)"
	@MTL_DEBUG_LAYER=1 ./$(OUT)

clean:
	@echo "=== Cleaning build artifacts ==="
	@echo "Removing: $(OUT) $(METAL_AIR) $(METAL_LIB) $(CSV_FILES)"
	rm -f $(OUT) $(METAL_AIR) $(METAL_LIB) $(CSV_FILES)
	@echo "✓ Clean completed"
