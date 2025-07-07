#include <algorithm>
#include <cassert>
#include <print>
#include <random>
#include <vector>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "../Metal.hpp"

#include <benchmark/benchmark.h>

using Matrix = std::vector<float>;

const auto MATRIX_DIMENSION = 1024;
const size_t MATRIX_BUFFER_SIZE =
  MATRIX_DIMENSION * MATRIX_DIMENSION * sizeof(float);

auto genMatrix (
  uint rows = MATRIX_DIMENSION,
  uint cols = MATRIX_DIMENSION) -> Matrix {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> distr(-5.0f, 5.0f);

  Matrix matrix(rows * cols);
  for (float& val : matrix) {
      val = static_cast<float>(distr(gen));
  }
  return matrix;
};

auto matMultiplicationMetal (Matrix& a, Matrix& b) -> Matrix {
  Matrix result(MATRIX_DIMENSION * MATRIX_DIMENSION, 0.0f);

  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
  // auto mtlStruct = MetalObjects (
  //   static_cast<std::filesystem::path>("./mat_mul.metallib"), "mat_mul");

  MTL::Device* pDevice = MTL::CreateSystemDefaultDevice();
  MTL::Library* pLibrary = pDevice->newLibrary (
    NS::String::string("./mat_mul.metallib",
    NS::StringEncoding::UTF8StringEncoding),
    nullptr);

  if (!pLibrary) {
    throw std::runtime_error("Couldn't find the .metallib file");
  }

  MTL::Function* pFunction = pLibrary->newFunction(
    NS::String::string("mat_mul", NS::UTF8StringEncoding));

  MTL::ComputePipelineState* pComputePipelineState =
    pDevice->newComputePipelineState(pFunction, (NS::Error**)nullptr);

  pLibrary->release();
  pFunction->release();

  MTL::Buffer
    *pBufferA = pDevice->newBuffer( a.data(),
                                    MATRIX_BUFFER_SIZE,
                                    MTL::ResourceStorageModeShared),
    *pBufferB = pDevice->newBuffer( b.data(),
                                    MATRIX_BUFFER_SIZE,
                                    MTL::ResourceStorageModeShared),
    *pBufferResult = pDevice->newBuffer(
                                    result.data(),
                                    MATRIX_BUFFER_SIZE,
                                    MTL::ResourceStorageModeShared);

  MTL::CommandQueue*  pCommandQueue = pDevice->newCommandQueue();
  MTL::CommandBuffer* pCommandBuffer = pCommandQueue->commandBuffer();
  MTL::ComputeCommandEncoder*
    pCommandEncoder = pCommandBuffer->computeCommandEncoder();

  pCommandEncoder->setComputePipelineState(pComputePipelineState);
  pCommandEncoder->setBuffer(pBufferA,      0, 0);
  pCommandEncoder->setBuffer(pBufferB,      0, 1);
  pCommandEncoder->setBuffer(pBufferResult, 0, 2);

  uint32_t matrix_inner_dim = MATRIX_DIMENSION;
  MTL::Buffer* pBufferDim = pDevice->newBuffer(&matrix_inner_dim,
                                               sizeof(uint32_t),
                                               MTL::ResourceStorageModeShared);
  pCommandEncoder->setBuffer(pBufferDim, 0, 3);

  MTL::Size threadsPerThreadgroup = MTL::Size(16, 16, 1);
  MTL::Size numGroups = MTL::Size(
    (MATRIX_DIMENSION + 15) / 16,
    (MATRIX_DIMENSION + 15) / 16, 1);

  pCommandEncoder->dispatchThreadgroups(numGroups, threadsPerThreadgroup);
  pCommandEncoder->endEncoding();
  pCommandBuffer->commit();
  pCommandBuffer->waitUntilCompleted();

  float* pResult = static_cast<float*>(pBufferResult->contents());
  std::copy(pResult, pResult + (MATRIX_DIMENSION * MATRIX_DIMENSION), result.begin());

  pBufferDim->release();
  pDevice->release();
  pBufferA->release();
  pBufferB->release();
  pBufferResult->release();
  return result;
}

static void BM_Metal (benchmark::State& state) {
  Matrix a = genMatrix();
  Matrix b = genMatrix();
  for (auto _ : state) {
    matMultiplicationMetal(a, b);
  }
}
BENCHMARK(BM_Metal);

auto matMultiplicationCPU (Matrix& a, Matrix& b) -> Matrix {
  Matrix result(MATRIX_DIMENSION * MATRIX_DIMENSION, 0.0f);
  for (uint32_t i = 0; i < MATRIX_DIMENSION; ++i) {
    for (uint32_t j = 0; j < MATRIX_DIMENSION; ++j) {
      for (uint32_t k = 0; k < MATRIX_DIMENSION; ++k) {
        result[i * MATRIX_DIMENSION + j] +=
          a[i * MATRIX_DIMENSION + k] * b[k * MATRIX_DIMENSION + j];
      }
    }
  }
  return result;
}

static void BM_CPU (benchmark::State& state) {
  Matrix a = genMatrix();
  Matrix b = genMatrix();
  for (auto _ : state) {
    matMultiplicationCPU(a, b);
  }
}
BENCHMARK(BM_CPU);

auto main (int argc, char* argv[]) -> int {
  benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
