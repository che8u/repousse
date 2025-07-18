// * This is only the shader source code, not the complete program.
// * This will be compiled by the Xcode bundle. - Don't use cpp features here.

#include <metal_stdlib>
using namespace metal;

kernel void vector_add(
  // ! Param order in buffer index is significant. Do not swap args.
  device const float* inA   [[ buffer(0) ]],
  device const float* inB   [[ buffer(1) ]],
  device       float* out   [[ buffer(2) ]],

  // * Per MSL Specv4: "When a kernel function is submitted for execution, it
  // * executes over an N-dimensional grid of threads, [...] A thread is an
  // * instance of the kernel function that executes for each point in this
  // * grid, and thread_position_in_grid identifies its position in the grid."
  // *        ▼
  uint gid [[ thread_position_in_grid ]]
) {
  out[gid] = inA[gid] + inB[gid];
}
