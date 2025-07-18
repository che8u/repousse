#include <simd/simd.h>

struct FractalParams {
  int iterations;
  simd::float2 offset;
  float zoom;
};
