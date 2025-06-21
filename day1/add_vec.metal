// * This is only the shader source code, not the complete program.
// * This will be compiled by the Xcode bundle. - Don't use cpp features here.

#include <metal_stdlib>
using namespace metal;

kernel void vector_add(
  device const float* inA   [[ buffer(0) ]],
  device const float* inB   [[ buffer(1) ]],
  device       float* out   [[ buffer(2) ]],
  uint          gid         [[ thread_position_in_grid ]])
{
  out[gid] = inA[gid] + inB[gid];
}
