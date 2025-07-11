#include <metal_stdlib>

kernel void golBuffer (
  device const uint32_t* input_grid [[buffer(0)]],
  device uint32_t* output_grid      [[buffer(1)]],

  constant uint16_t& grid_width     [[buffer(2)]],
  constant uint16_t& grid_height    [[buffer(3)]],
  uint2 thread_id                   [[thread_position_in_grid]]) {
  if (thread_id.x >= grid_width || thread_id.y >= grid_height) { return; }

  uint16_t current_idx = thread_id.x + thread_id.y * grid_width;
  uint16_t live_neighbors = 0;

  for (int i = -1; i <= 1; ++i) {
    for (int j = -1; j <= 1; ++j) {
      if (i == 0 && j == 0) { continue; }

      uint16_t neighbor_x = (thread_id.x + i + grid_width)  % grid_width;
      uint16_t neighbor_y = (thread_id.y + j + grid_height) % grid_height;

      // * Convert two to one-dimension for `buffer`
      uint16_t neighbor_idx = neighbor_x + neighbor_y * grid_width;

      // * fetch state of cell located at ▼
      live_neighbors += input_grid[neighbor_idx];
    }
  }

  uint32_t current_state = input_grid[current_idx];
  uint32_t new_state = 0;

  // * Rules:
  // * Case 1. Live cell with less than 2 neighbours dies
  // * Case 2. Live cell with exactly 2 or 3 neighbours lives
  // * Case 3. Live cell with more than 3 neighbours dies
  // * Case 4. Dead cell with exactly 3 neighbours becomes live
  if ((current_state == 1) && (live_neighbors == 2 || live_neighbors == 3)) {
    // Case 2: Live cell with 2 or 3 neighbors survives
    new_state = 1;
  } else if ((current_state == 0) && live_neighbors == 3) {
    // Case 4: Dead cell with exactly 3 neighbors becomes alive
    new_state = 1;
  } else {
    // Case 1&3: The cell either dies or stays dead
    new_state = 0;
  }

  output_grid[current_idx] = new_state;
  return;
}
