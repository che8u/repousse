#include <metal_stdlib>
#include "fractal_params.hpp"
using namespace metal;

kernel void fractal (
  const device FractalParams &params          [[buffer(0)]],
  texture2d<float, access::write> opTexture   [[texture(0)]],
  uint2 threadID                              [[thread_position_in_grid]]) {
  auto gridWidth  = opTexture.get_width();
  auto gridHeight = opTexture.get_height();

  // * Map the pixel coordinates to the texture
  // * The coordinates are normalized to be in the range [-2, 2]
  float2 pixelCoords = float2(threadID) / float2(gridWidth, gridHeight);
  pixelCoords = ((pixelCoords - 0.5) * 2.0 * (params.zoom)) - params.offset;

  // Mandelbrot set calculation
  float2 z = 0; // represents complex number to be iterated on
  int i = 0;
  for (i = 0; i < params.iterations; ++i) {
    z = float2(z.x * z.x - z.y * z.y, 2 * z.x * z.y) + pixelCoords;
    // * Escape condition: point escapes the circle of radius 2
    if (dot(z, z) > 4.0f) { break; }
    
    // * If point is inside the set, make it white and exit.
    if (i == params.iterations) {
      opTexture.write(float4(1.0, 1.0, 1.0, 1.0), threadID);
      return;
    }
  }

  // * For points outside the set, calculate a smooth color value.
  float smooth_i = float(i) + 1.0f - log(log(length(z))) / log(2.0f);

  // * Map the smooth value to a color palette using sine waves.
  float t = 0.1f * smooth_i;
  float r = 0.5f + 0.5f * sin(t + 0.0f);
  float g = 0.5f + 0.5f * sin(t + 2.094f); // 120 degrees phase shift
  float b = 0.5f + 0.5f * sin(t + 4.188f); // 240 degrees phase shift
  opTexture.write(float4(r, g, b, 1.0f), threadID);
  return;
}
