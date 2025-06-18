#include <print>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "../Metal.hpp"

int main() {
  // std::println("Hello, World!");
  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();

  // * GPU device abstraction
  // Ref: https://developer.apple.com/documentation/metal/performing-calculations-on-a-gpu?language=objc#Find-a-GPU
  MTL::Device* pDevice = MTL::CreateSystemDefaultDevice();

  if (pDevice) {
    std::println("This device has a Metal-compatible GPU. 👯‍♀️");
  } else {
    std::println("No GPU device found 😬");
  }

  pDevice->release();
  pAutoReleasePool->release();
  return 0;
}
