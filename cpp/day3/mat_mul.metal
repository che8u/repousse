#include <metal_stdlib>
using namespace metal;

// Size of the tile and threadgroup. This must match the threadgroup
// size set in the C++ host code. Use smaller tile size for it to fit
// within the GPU's caches.
#define TILE_SIZE 16

kernel void mat_mul (
  device const float* matrix_a [[buffer(0)]],
  device const float* matrix_b [[buffer(1)]],
  device float* result_matrix  [[buffer(2)]],

  // gid: Global thread ID
  // tid: Local thread ID within threadgroup
  // tgroup_id: ID of the threadgroup itself within grid of groups.
  constant uint& matrix_inner_dim [[buffer(3)]],
  uint2 gid [[thread_position_in_grid]],
  uint2 tid [[thread_position_in_threadgroup]],
  uint2 tgroup_id [[threadgroup_position_in_grid]],
  uint2 grid_size [[threads_per_grid]])
  // grid_size.x = width of matrix B and the result matrix.
  // grid_size.y = height of matrix A and the result matrix.
  // The width of A and height of B are assumed to be equal (inner dimension).
{
  // Prevent threads from writing out of bounds if the
  // matrix dimensions are not perfectly divisible by the tile size.
  if (gid.x >= grid_size.x || gid.y >= grid_size.y) { return; }

  threadgroup float tileA[TILE_SIZE][TILE_SIZE];
  threadgroup float tileB[TILE_SIZE][TILE_SIZE];

  float sum = 0.0f;
  // Process matrices one tile at a time.
  // The number of tiles is the total inner dimension divided by the tile size.
  uint num_tiles = (matrix_inner_dim + TILE_SIZE - 1) / TILE_SIZE;
  for (uint tile_idx = 0; tile_idx < num_tiles; ++tile_idx) {
    // Calculate the source indices in the global matrices.
    const uint a_col = tile_idx * TILE_SIZE + tid.x;
    const uint a_row = gid.y;

    const uint b_col = gid.x;
    const uint b_row = tile_idx * TILE_SIZE + tid.y;

    // Load the elements into the shared tiles.
    // Perform a bounds check for cases where matrix dimensions aren't a
    // multiple of TILE_SIZE, preventing reads from out of bounds.
    if (a_col < matrix_inner_dim) {
      tileA[tid.y][tid.x] = matrix_a[a_row * matrix_inner_dim + a_col];
    } else {
      tileA[tid.y][tid.x] = 0.0f;
    }

    if (b_row < matrix_inner_dim) {
      tileB[tid.y][tid.x] = matrix_b[b_row * grid_size.x + b_col];
    } else {
      tileB[tid.y][tid.x] = 0.0f;
    }

    // Ensures that all threads in the threadgroup have finished loading their
    // data into tileA and tileB
    threadgroup_barrier(mem_flags::mem_threadgroup);

    // Each thread performs dot product for its portion of the tile.
    for (uint k = 0; k < TILE_SIZE; ++k) {
      sum += tileA[tid.y][k] * tileB[k][tid.x];
    }

    // Ensures all threads have finished their calculations with the current
    // tile before the next iteration
    threadgroup_barrier(mem_flags::mem_threadgroup);
  }

  result_matrix[gid.y * grid_size.x + gid.x] = sum;
}
