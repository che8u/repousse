#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <print>
#include <random>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <unistd.h>
#include <benchmark/benchmark.h>
#include <sys/types.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "../Metal.hpp"

using grid = std::vector<uint32_t>;

const uint16_t GRID_WIDTH  = 512;
const uint16_t GRID_HEIGHT = 512;
const uint16_t GENERATIONS = 100;

auto writeToCsv (const grid& g, const std::filesystem::path path) -> void {
  std::ofstream outFile(path);
  if (!outFile.is_open()) {
    throw std::runtime_error("Could not open CSV file for writing.");
  }
  for (uint32_t y = 0; y < GRID_HEIGHT; ++y) {
    for (uint32_t x = 0; x < GRID_WIDTH; ++x) {
      outFile << static_cast<int>(g.at(y * GRID_WIDTH + x)) << (x == GRID_WIDTH - 1 ? "" : ",");
    }
    outFile << "\n";
  }
}

auto genInitialGrid (
  uint16_t width =  GRID_WIDTH,
  uint16_t height = GRID_HEIGHT) -> grid {
  grid output(width * height);
  std::mt19937 gen(1337);
  std::uniform_real_distribution<> dis(0.0, 1.0);
  // * 20% chance for a cell to be alive
  for (auto& cell : output) { cell = (dis(gen) < 0.2) ? 1 : 0; }
  return output;
}

auto golSimBuffer (
  const grid& initialGrid,
  const uint16_t generations,
  const std::function<void(const grid&, uint16_t)>& frameSaver) -> grid {
  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
  MTL::Device* pDevice = MTL::CreateSystemDefaultDevice();
  MTL::Library* pLibrary = pDevice->newLibrary(
    NS::String::string("./gol_buffer.metallib", NS::StringEncoding::UTF8StringEncoding),
    nullptr);

  if (!pLibrary) {
    throw std::runtime_error("Couldn't find the .metallib file.");
  }

  MTL::Function* pFunction = pLibrary->newFunction(
    NS::String::string("golBuffer", NS::StringEncoding::UTF8StringEncoding));
  MTL::ComputePipelineState* pComputePipelineState =
    pDevice->newComputePipelineState(pFunction, (NS::Error**)nullptr);

  MTL::CommandQueue* pCommandQueue = pDevice->newCommandQueue();

  const size_t bufferSize = GRID_WIDTH * GRID_HEIGHT * sizeof(uint32_t);
  MTL::Buffer* pReadBuffer = pDevice->newBuffer(bufferSize, MTL::ResourceStorageModeManaged);
  MTL::Buffer* pWriteBuffer = pDevice->newBuffer(bufferSize, MTL::ResourceStorageModeManaged);

  std::copy(initialGrid.begin(), initialGrid.end(), static_cast<uint32_t*>(pReadBuffer->contents()));
  pReadBuffer->didModifyRange(NS::Range(0, bufferSize));

  MTL::Size threadsPerThreadgroup = MTL::Size(16, 16, 1);
  MTL::Size numGroups = MTL::Size((GRID_WIDTH + 15) / 16, (GRID_HEIGHT + 15) / 16, 1);
  grid frameGrid(GRID_WIDTH * GRID_HEIGHT);

  for (uint16_t i = 0; i < generations; ++i) {
    MTL::CommandBuffer* pCommandBuffer = pCommandQueue->commandBuffer();

    MTL::ComputeCommandEncoder* pCommandEncoder = pCommandBuffer->computeCommandEncoder();
    pCommandEncoder->setComputePipelineState(pComputePipelineState);
    pCommandEncoder->setBuffer(pReadBuffer,  0, 0);
    pCommandEncoder->setBuffer(pWriteBuffer, 0, 1);
    pCommandEncoder->setBytes(&GRID_WIDTH, sizeof(uint16_t), 2);
    pCommandEncoder->setBytes(&GRID_HEIGHT, sizeof(uint16_t), 3);
    pCommandEncoder->dispatchThreadgroups(numGroups, threadsPerThreadgroup);
    pCommandEncoder->endEncoding();

    pCommandBuffer->commit();
    pCommandBuffer->waitUntilCompleted();

    std::swap(pReadBuffer, pWriteBuffer);

    auto* bufferContents = static_cast<uint32_t*>(pReadBuffer->contents());
    std::copy(bufferContents, bufferContents + initialGrid.size(), frameGrid.begin());
    if (frameSaver) frameSaver(frameGrid, i + 1);
  }

  auto* bufferContents = static_cast<uint32_t*>(pReadBuffer->contents());
  std::copy(bufferContents, bufferContents + initialGrid.size(), frameGrid.begin());

  pReadBuffer->release();
  pWriteBuffer->release();
  pCommandQueue->release();
  pComputePipelineState->release();
  pFunction->release();
  pLibrary->release();
  pDevice->release();
  return frameGrid;
}

auto golSimTexture(
  const grid& initialGrid,
  const uint16_t generations,
  const std::function<void(const grid&, uint16_t)>& frameSaver) -> grid {
  NS::AutoreleasePool* pAutoReleasePool = NS::AutoreleasePool::alloc()->init();
  MTL::Device* pDevice = MTL::CreateSystemDefaultDevice();

  MTL::Library* pLibrary = pDevice->newLibrary(
  NS::String::string("./gol_texture.metallib", NS::StringEncoding::UTF8StringEncoding),
    nullptr);

  if (!pLibrary) {
    throw std::runtime_error("Couldn't find the .metallib file.");
  }

  MTL::Function* pFunction = pLibrary->newFunction(
    NS::String::string("golTexture", NS::StringEncoding::UTF8StringEncoding));
  MTL::ComputePipelineState* pComputePipelineState =
    pDevice->newComputePipelineState(pFunction, (NS::Error**)nullptr);

  MTL::TextureDescriptor* pTextureDesc = MTL::TextureDescriptor::alloc()->init();
  pTextureDesc->setPixelFormat(MTL::PixelFormatR32Uint);
  pTextureDesc->setWidth(static_cast<NS::UInteger>(GRID_WIDTH));
  pTextureDesc->setHeight(static_cast<NS::UInteger>(GRID_HEIGHT));
  pTextureDesc->setStorageMode(MTL::StorageModeManaged);
  pTextureDesc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);

  MTL::Texture* pReadTexture  = pDevice->newTexture(pTextureDesc);
  MTL::Texture* pWriteTexture = pDevice->newTexture(pTextureDesc);
  MTL::Region region = MTL::Region::Make2D(0, 0, GRID_WIDTH, GRID_HEIGHT);
  pReadTexture->replaceRegion(region, 0, initialGrid.data(), GRID_WIDTH * sizeof(uint32_t));

  MTL::CommandQueue* pCommandQueue = pDevice->newCommandQueue();
  MTL::Size threadsPerThreadgroup = MTL::Size(16, 16, 1);
  MTL::Size numGroups = MTL::Size((GRID_WIDTH + 15) / 16, (GRID_HEIGHT + 15) / 16, 1);
  grid frameGrid(GRID_WIDTH * GRID_HEIGHT);

  for (uint16_t i = 0; i < generations; ++i) {
    MTL::CommandBuffer* pCommandBuffer = pCommandQueue->commandBuffer();

    MTL::ComputeCommandEncoder* pCommandEncoder = pCommandBuffer->computeCommandEncoder();
    pCommandEncoder->setComputePipelineState(pComputePipelineState);
    pCommandEncoder->setTexture(pReadTexture,  0);
    pCommandEncoder->setTexture(pWriteTexture, 1);
    pCommandEncoder->dispatchThreadgroups(numGroups, threadsPerThreadgroup);
    pCommandEncoder->endEncoding();

    pCommandBuffer->commit();
    pCommandBuffer->waitUntilCompleted();

    std::swap(pReadTexture, pWriteTexture);

    pReadTexture->getBytes(frameGrid.data(), GRID_WIDTH * sizeof(uint32_t), region, 0);
    if (frameSaver) frameSaver(frameGrid, i + 1);
  }

  pReadTexture->getBytes(frameGrid.data(), GRID_WIDTH * sizeof(uint32_t), region, 0);

  pReadTexture->release();
  pWriteTexture->release();
  pTextureDesc->release();
  pCommandQueue->release();
  pComputePipelineState->release();
  pFunction->release();
  pLibrary->release();
  pDevice->release();
  return frameGrid;
}

auto main (int argc, char** argv) -> int {
  const grid initialGrid = genInitialGrid();
  const std::string bufferOutputDir  = "gol_frames_buffer";
  const std::string textureOutputDir = "gol_frames_texture";
  std::filesystem::create_directory(bufferOutputDir);
  std::filesystem::create_directory(textureOutputDir);

  auto saveFrame = [](const std::string& dir, const grid& g, uint32_t frameNum) {
    std::stringstream ss;
    ss << dir << "/frame_" << std::setw(4) << std::setfill('0') << frameNum << ".csv";
    writeToCsv(g, ss.str());
  };

  // Save initial state (frame 0) for both
  saveFrame(bufferOutputDir, initialGrid, 0);
  saveFrame(textureOutputDir, initialGrid, 0);

  benchmark::RegisterBenchmark("BM_Buffer", [&](benchmark::State& state) {
    auto frameSaver = [&](const grid& g, uint16_t frameNum) {
      saveFrame(bufferOutputDir, g, frameNum);
    };
    for (auto _ : state) {
      golSimBuffer(initialGrid, GENERATIONS, frameSaver);
    }
  });

  benchmark::RegisterBenchmark("BM_Texture", [&](benchmark::State& state) {
    auto frameSaver = [&](const grid& g, uint16_t frameNum){
      saveFrame(textureOutputDir, g, frameNum);
    };
    for (auto _ : state) {
      golSimTexture(initialGrid, GENERATIONS, frameSaver);
    }
  });

  benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();

  // ! This doesn't work
  // std::println("Validating results...");
  // std::string diff_cmd = "diff -r " + bufferOutputDir + " " + textureOutputDir;
  // int result = std::system(diff_cmd.c_str());
  // if (result == 0) { std::println("Results match! 🍻"); }
  // else { std::println("Results do not match 🤦"); }
  return 0;
}
