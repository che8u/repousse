#include <metal_stdlib>

using namespace metal;

kernel void golTexture (
  // * Permissible values ▼ : `read`, `write`, `read_write`, `sample`
  texture2d<uint, access::read>  input_texture  [[texture(0)]],
  texture2d<uint, access::write> output_texture [[texture(1)]],
  // *      ▲ can't be uint8_t; must be uint/half/float ...

  // * Simpler to infer from texture dimensions
  // constant uint& grid_width  [[buffer(0)]],
  // constant uint& grid_height [[buffer(1)]],
  uint2 thread_id [[thread_position_in_grid]]) {
  uint16_t grid_width  = input_texture.get_width();
  uint16_t grid_height = input_texture.get_height();
  if (thread_id.x >= grid_width || thread_id.y >= grid_height) { return; }

  uint16_t live_neighbors = 0;
  for (int i = -1; i <= 1; ++i) {
    for (int j = -1; j <= 1; ++j) {
      if (i == 0 && j == 0) { continue; }

      uint16_t neighbor_x = (thread_id.x + i + grid_width) % grid_width;
      uint16_t neighbor_y = (thread_id.y + j + grid_height) % grid_height;

      // The '.x' swizzle gets the first channel, which holds our 0 or 1 value.
      uint32_t neighbor_state = static_cast<uint32_t>(input_texture.read(uint2(neighbor_x, neighbor_y)).x);
      live_neighbors += neighbor_state;
    }
  }

  uint32_t current_state = input_texture.read(thread_id).r;
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


  // output_texture.write(uint4(new_state, 0, 0, 1), thread_id);
  output_texture.write(new_state, thread_id);
  return;
}
