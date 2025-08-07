APPNAME := genBarcode
SRC_DIR := barcode
BUILD_DIR := build/
METAL_DIR   := ../

OUT := $(APPNAME)

SRC     := $(wildcard $(SRC_DIR)/*.cc)

CXX := clang++
INCLUDES := $(METAL_DIR) $(wildcard $(SRC_DIR)/*.h)
CXXFLAGS    := -std=c++23 -fobjc-arc -O3 -march=native -flto -I/opt/homebrew/include
CXXFLAGS    += $(INCLUDES:%=-I%)
LDFLAGS     := -framework Metal

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	@echo "Building C++ executable..."
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$@ $(SRC) $(LDFLAGS)

clean:
	@echo "Cleaning up..."
	@if [ -d "$(BUILD_DIR)" ]; then rm -rf $(BUILD_DIR)/* $(BUILD_DIR)/.[!.]* $(BUILD_DIR)/..?*; fi
