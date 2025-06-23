#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <print>
#include <stdexcept>
#include <array>
#include <cmath>
#include <format>
#include <filesystem>
#include <span>
#include <string_view>
#include <fstream>

#include <benchmark/benchmark.h>
#include <sys/types.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "../Metal.hpp"

constexpr float PI = 3.14159265358979323846f;

constexpr size_t INPUT_SIZE = 2048;
constexpr size_t MASK_SIZE  = 31;  // ! Must be odd
constexpr float SIGMA       = 2.0f;

///////////////////////////////////////////////////////////////////////////////
// * Function to print to CSV:

void writeToCSV (
  std::span<const float> data,
  std::string_view arr_name
) {
  const std::filesystem::path path = std::format("{}.csv", arr_name);
  std::println(
    "Writing {} data points to '{}'...",
    data.size(),
    path.string());
  std::ofstream file(path);

  // * header for csv
  std::println(file, "Index,Value");

  for (size_t i = 0; i < data.size(); ++i) {
    std::println(file, "{},{}", i, data[i]);
  }

  std::println("Finished writing to '{}'.\n", path.string());
}

///////////////////////////////////////////////////////////////////////////////

// std::vector<float> genTestSignal(size_t len) {
auto genTestSignal() -> std::array<float, INPUT_SIZE> {
  std::array<float, INPUT_SIZE> signal{};
  for (size_t i = 0; i < INPUT_SIZE; ++i) {
    float t = static_cast<float>(i);
    signal[i] =  std::sin(2.0f * PI * t / 10.0f);   // * high-freq component
    signal[i] += std::sin(2.0f * PI * t / 50.0f);   // * medium-freq component
    signal[i] += std::sin(2.0f * PI * t / 100.0f);  // * low-freq component
  }
  return signal;
}
auto def_signal = genTestSignal();

// std::vector<float> genMask(size_t len, float sigma = SIGMA) {
auto genMask() -> std::array<float, MASK_SIZE> {
  int center = static_cast<int>(MASK_SIZE / 2);
  std::array<float, MASK_SIZE> mask{};
  float sum = 0.0f;

  for (int i = 0; i < MASK_SIZE; ++i) {
    int x = i - center;
    float val = std::exp(
      -1 * static_cast<float>(x * x) / (2.0f * SIGMA * SIGMA));
    mask[i] = val;
    sum += val;
  }

  // * Normalize the mask
  for (float& val : mask) {
    val /= sum;
  }
  return mask;
}
auto def_mask = genMask();

std::array<float, INPUT_SIZE> calculateConvolution(
  const std::array<float, INPUT_SIZE>& signal = def_signal,
  const std::array<float, MASK_SIZE>& mask = def_mask
) {
  /////////////////////////////////////////////////////////////////////////////
  // * Metal boilerplate ...
  /////////////////////////////////////////////////////////////////////////////
  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
  MTL::Device*  pDevice = MTL::CreateSystemDefaultDevice();
  // MTL::Library* pLibrary = pDevice->newLibrary(
  //   NS::String::string("./convolution.metallib", nullptr), nullptr);
  MTL::Library* pLibrary = pDevice->newLibrary(
    NS::String::string("./convolution.metallib", NS::ASCIIStringEncoding),
    (NS::Error**)nullptr);

  if (!pLibrary) {
    throw std::runtime_error("Couldn't find the .metallib file");
  }

  MTL::Function* pFunction = pLibrary->newFunction(
    NS::String::string("convolution", NS::UTF8StringEncoding));

  MTL::ComputePipelineState* pComputePipelineState =
    pDevice->newComputePipelineState(pFunction, (NS::Error**)nullptr);

  /////////////////////////////////////////////////////////////////////////////
  // * Function logic begins here ...
  /////////////////////////////////////////////////////////////////////////////

  std::array<float, INPUT_SIZE> output{};

  // * buffers
  MTL::Buffer* pInputBuf = pDevice->newBuffer(
    signal.data(), INPUT_SIZE * sizeof(float), MTL::ResourceStorageModeShared);
  MTL::Buffer* pMaskBuf = pDevice->newBuffer(
    mask.data(),    MASK_SIZE * sizeof(float), MTL::ResourceStorageModeShared);
  MTL::Buffer* pOutputBuf = pDevice->newBuffer(
                INPUT_SIZE * sizeof(float), MTL::ResourceStorageModeShared);
  // *          ▲ equivalent to output.size()

  MTL::CommandQueue*  pCommandQueue = pDevice->newCommandQueue();
  MTL::CommandBuffer* pCommandBuffer = pCommandQueue->commandBuffer();
  MTL::ComputeCommandEncoder*
    pCommandEncoder = pCommandBuffer->computeCommandEncoder();

  pCommandEncoder->setComputePipelineState(pComputePipelineState);
  pCommandEncoder->setBuffer(pInputBuf,  0, 0);
  pCommandEncoder->setBuffer(pMaskBuf,   0, 1);
  pCommandEncoder->setBuffer(pOutputBuf, 0, 2);

  uint32_t metal_mask_size = MASK_SIZE;
  pCommandEncoder->setBytes(&metal_mask_size, sizeof(metal_mask_size), 3);
  uint32_t metal_input_size = INPUT_SIZE;
  pCommandEncoder->setBytes(&metal_input_size, sizeof(metal_input_size), 4);


  MTL::Size threadsPerThreadgroup = MTL::Size(256, 1, 1);
  MTL::Size numGroups = MTL::Size((INPUT_SIZE+255)/256, 1, 1);
  // * Here, `threadsPerThreadgroup` is constant (=256).
  // * The total number of threads is    `numGroups * threadsPerThreadgroup`.
  // * Let total number of threads be T = numGroups * 256
  // * To ensure that every index of our `output` is processed by a thread,
  // * we must ensure that `T >= INPUT_SIZE`.
  //
  // o T = INPUT_SIZE ==> each thread worked on each element of `output` and
  //   that no thread was idle.
  // o T > INPUT_SIZE ==> threads up until `INPUT_SIZE` were used, the extra
  //   threads became idle after checking the bounds of the vector.

  // pCommandEncoder->dispatchThreads(numGroups, threadsPerThreadgroup);
  pCommandEncoder->dispatchThreads(MTL::Size(INPUT_SIZE, 1, 1), threadsPerThreadgroup);
  pCommandEncoder->endEncoding();
  pCommandBuffer->commit();
  pCommandBuffer->waitUntilCompleted();

  float* pOutput = static_cast<float*>(pOutputBuf->contents());
  std::copy(pOutput, pOutput+INPUT_SIZE, output.begin());
  /////////////////////////////////////////////////////////////////////////////
  // * Cleanup ...
  /////////////////////////////////////////////////////////////////////////////
  pDevice->release();
  pInputBuf->release();
  pMaskBuf->release();
  pOutputBuf->release();

  return output;
}

static void BM_Metal(benchmark::State& state) {
    for (auto _ : state) {
      writeToCSV(calculateConvolution(), "output_signal");
    }
}
BENCHMARK(BM_Metal);

//        ▼ I don't like doing this, but `benchmark` requires it
int main (int argc, char** argv) {
  writeToCSV(def_signal, "inp_signal");
  writeToCSV(def_mask, "mask_arr");

  auto output = calculateConvolution();
  writeToCSV(output, "output_signal");

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
// BENCHMARK_MAIN();
