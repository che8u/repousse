CXX = clang++
CXXFLAGS = -std=c++23 -fobjc-arc
LDFLAGS = -framework Foundation -framework Metal
SRC = main.cc
OUT = bin

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
