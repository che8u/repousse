APPNAME     := Fractal                       # single canonical name
SRC_DIR     := fractal
BUILD_DIR   := build
METAL_DIR   := ../

OUT         := $(BUILD_DIR)/$(APPNAME)        # Mach-O you link
METAL_LIB   := $(BUILD_DIR)/fractal.metallib

###############################################################################
# * Sources & intermediate files
###############################################################################
SRC         := $(wildcard $(SRC_DIR)/*.cc)
METAL_SRC   := $(wildcard $(SRC_DIR)/*.metal)
METAL_AIR   := $(BUILD_DIR)/fractal.air

###############################################################################
# * Tools & flags
###############################################################################
CXX         := clang++
INCLUDES    := $(METAL_DIR)
CXXFLAGS    := -std=c++23 -fobjc-arc -g -I/opt/homebrew/include
CXXFLAGS    += $(INCLUDES:%=-I%)
LDFLAGS     := -framework Foundation -framework Metal -framework Cocoa \
               -L/opt/homebrew/lib -lSDL3

.PHONY: all clean

###############################################################################
# * Default build
# The 'all' target now just builds the executable and the metal library.
###############################################################################
all: $(OUT) $(METAL_LIB)

$(METAL_AIR): $(METAL_SRC)
	@echo "Compiling Metal shader..."
	@mkdir -p $(@D)
	xcrun -sdk macosx metal -c $< -o $@

$(METAL_LIB): $(METAL_AIR)
	@echo "Linking Metal library..."
	xcrun -sdk macosx metallib $< -o $@

$(OUT): $(SRC) $(METAL_LIB)
	@echo "Building C++ executable..."
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

###############################################################################
# * misc.
###############################################################################
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
