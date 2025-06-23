#include <metal_stdlib>

kernel void convolution (
  device const float* input  [[buffer(0)]],
  device const float* mask   [[buffer(1)]],
  device float*       output [[buffer(2)]],

  constant uint& mask_width  [[buffer(3)]],
  constant uint& input_width [[buffer(4)]],

  // * Per MSL Specv4: "When a kernel function is submitted for execution, it
  // * executes over an N-dimensional grid of threads, [...] A thread is an
  // * instance of the kernel function that executes for each point in this
  // * grid, and thread_position_in_grid identifies its position in the grid."
  // *             ▼
  uint thread_id [[thread_position_in_grid]]
) {
  if (thread_id >= input_width) {
    return;
  }
  const int center = static_cast<int>(mask_width/2);

  // * Convolution
  float sum = 0.0f;
  for (uint i = 0u; i < mask_width; i++) {
    const int offset = static_cast<int>(i) - center;
    const int idx = static_cast<int>(thread_id) + offset;

    if (idx >= 0 && idx < static_cast<int>(input_width)) {
      sum += input[uint(idx)] * mask[i];
    }
  }
  output[thread_id] = sum;
}
