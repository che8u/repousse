#include <print>
#include <stdexcept>
#include <vector>
#include <random>

#include <benchmark/benchmark.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "../Metal.hpp"

std::vector<float> genVec (unsigned int vecLength) {
  std::vector<float> v;
  v.reserve(vecLength);
  std::mt19937 gen(42);
  std::uniform_real_distribution<float> dis(0.0f, 1.0f);
  for (unsigned int i = 0; i < vecLength; ++i) {
    v.push_back(dis(gen));
  }
  return v;
}

void usingMetal(unsigned int vecLength) {
  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
  MTL::Device* pDevice = MTL::CreateSystemDefaultDevice();

  // * Step 1: Load the Metal library from file
  NS::Error* pError = nullptr;
  MTL::Library* pLibrary = pDevice->newLibrary(NS::String::string("./add_vec.metallib", NS::UTF8StringEncoding), &pError);
  if (!pLibrary) {
    std::println("Failed to load Metal library: {}", pError ? pError->localizedDescription()->utf8String() : "Unknown error");
    pDevice->release();
    pAutoReleasePool->release();
    return;
  }

  // * Step 2: Get the compute function
  MTL::Function* pFunction = pLibrary->newFunction(NS::String::string("vector_add", NS::UTF8StringEncoding));
  if (!pFunction) {
    std::println("Failed to get compute function 'vector_add'");
    pLibrary->release();
    pDevice->release();
    pAutoReleasePool->release();
    return;
  }

  // * Step 3: Create the compute pipeline state
  MTL::ComputePipelineState* pComputePipelineState = pDevice->newComputePipelineState(pFunction, &pError);
  if (!pComputePipelineState) {
    std::println("Failed to create compute pipeline state: {}", pError ? pError->localizedDescription()->utf8String() : "Unknown error");
    pFunction->release();
    pLibrary->release();
    pDevice->release();
    pAutoReleasePool->release();
    return;
  }

  // * Step 4: Create a command queue
  MTL::CommandQueue* pCommandQueue = pDevice->newCommandQueue();

  // * Step 5: Create input and output buffers
  std::vector<float> vectorA = genVec(vecLength);
  std::vector<float> vectorB = genVec(vecLength);
  std::vector<float> vectorC(vecLength, 0.0f);

  MTL::Buffer* pBufferA = pDevice->newBuffer(vectorA.data(), vectorA.size() * sizeof(float), MTL::ResourceStorageModeShared);
  MTL::Buffer* pBufferB = pDevice->newBuffer(vectorB.data(), vectorB.size() * sizeof(float), MTL::ResourceStorageModeShared);
  MTL::Buffer* pBufferC = pDevice->newBuffer(vectorC.data(), vectorC.size() * sizeof(float), MTL::ResourceStorageModeShared);

  // * Step 6: Create command buffer and encoder
  MTL::CommandBuffer* pCommandBuffer = pCommandQueue->commandBuffer();
  MTL::ComputeCommandEncoder* pCommandEncoder = pCommandBuffer->computeCommandEncoder();

  // * Step 7: Set pipeline state and buffers
  pCommandEncoder->setComputePipelineState(pComputePipelineState);
  pCommandEncoder->setBuffer(pBufferA, 0, 0);
  pCommandEncoder->setBuffer(pBufferB, 0, 1);
  pCommandEncoder->setBuffer(pBufferC, 0, 2);

  // * Step 8: Dispatch threads
  MTL::Size gridSize(vecLength, 1, 1);
  NS::UInteger threadGroupSize = pComputePipelineState->maxTotalThreadsPerThreadgroup();
  if (threadGroupSize > vecLength) threadGroupSize = vecLength;
  MTL::Size threadgroupSize(threadGroupSize, 1, 1);
  pCommandEncoder->dispatchThreads(gridSize, threadgroupSize);

  pCommandEncoder->endEncoding();

  // * Step 9: Commit and wait
  pCommandBuffer->commit();
  pCommandBuffer->waitUntilCompleted();

  // * Step 10: Cleanup
  pBufferA->release();
  pBufferB->release();
  pBufferC->release();
  pCommandQueue->release();
  pComputePipelineState->release();
  pFunction->release();
  pLibrary->release();
  pDevice->release();
  pAutoReleasePool->release();
}

void usingCPU(unsigned int vecLength) {
  std::vector<float> vectorA = genVec(vecLength);
  std::vector<float> vectorB = genVec(vecLength);
  std::vector<float> vectorC(vecLength);

  for (size_t i = 0; i < vecLength; ++i) {
    vectorC[i] = vectorA[i] + vectorB[i];
  }
  return;
}

static void BM_Metal(benchmark::State& state) {
  unsigned int vecLength = state.range(0);
  for (auto _ : state) {
    usingMetal(vecLength);
  }
}

static void BM_CPU(benchmark::State& state) {
  unsigned int vecLength = state.range(0);
  for (auto _ : state) {
    usingCPU(vecLength);
  }
}

const long long start = 100000000;
const long long end   = start*2;
const long long step  = start;

// * Value order to follow:     ▼ `start, end, step`
BENCHMARK(BM_CPU)  ->DenseRange(start, end, step);
BENCHMARK(BM_Metal)->DenseRange(start, end, step);
BENCHMARK_MAIN();
