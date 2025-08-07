#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Metal.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct RGBA { uint8_t r, g, b, a; };
constexpr int CHANNELS = 4; // RGBA

/**
 * @brief creates a MTL::Texture from a raw pixel buffer.
 * @param pDevice pointer to the MTL::Device.
 * @param pixelBuffer A vector containing the raw RGBA pixel data.
 * @param width of the image
 * @param height of the image
 */
auto createTextureFromBuffer(
  const NS::SharedPtr<MTL::Device>& pDevice,
  const std::vector<uint8_t>& pixelBuffer,
  const unsigned int width,
  const unsigned int height) -> NS::SharedPtr<MTL::Texture> {
  auto pTexDesc = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
  pTexDesc->setWidth(width);
  pTexDesc->setHeight(height);
  pTexDesc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
  pTexDesc->setTextureType(MTL::TextureType2D);
  pTexDesc->setStorageMode(MTL::StorageModeManaged);
  pTexDesc->setUsage(MTL::TextureUsageShaderRead);

  // number of mipmap levels required to get to a 1x1 texture
  size_t maxDim = std::max(width, height);
  pTexDesc->setMipmapLevelCount(static_cast<size_t>(log2(maxDim)) + 1);

  auto pTexture = NS::TransferPtr(pDevice->newTexture(pTexDesc.get()));
  if (!pTexture) { return nullptr; }

  // pixel data goes from CPU buffer to the texture
  MTL::Region region = MTL::Region::Make2D(0, 0, width, height);
  NS::UInteger bytesPerRow = CHANNELS * width;
  pTexture->replaceRegion(region, 0, pixelBuffer.data(), bytesPerRow);
  return pTexture;
}

/**
 * @brief Uses mipmap generation to calculate the average color of a texture.
 * @param pCommandQueue A command queue to submit GPU work.
 * @param pTexture The texture to analyze.
 * @return The average RGBA color of the texture.
 */
auto calculateAverageColor(
  const NS::SharedPtr<MTL::CommandQueue>& pCommandQueue,
  const NS::SharedPtr<MTL::Texture>& pTexture) -> RGBA {
  auto pCmdBuffer = NS::TransferPtr(pCommandQueue->commandBuffer());

  auto pBlitEncoder = NS::TransferPtr(pCmdBuffer->blitCommandEncoder());
  pBlitEncoder->generateMipmaps(pTexture.get());
  pBlitEncoder->endEncoding();
  pCmdBuffer->commit();
  pCmdBuffer->waitUntilCompleted();

  // last mipmap level is a 1x1 texture holding the average color. Read it back.
  RGBA avgColor;
  MTL::Region region = MTL::Region::Make2D(0, 0, 1, 1);
  NS::UInteger bytesPerRow = CHANNELS * 1;
  size_t lastMipLevel = pTexture->mipmapLevelCount() - 1;

  pTexture->getBytes(&avgColor, bytesPerRow, region, lastMipLevel);
  return avgColor;
}

/**
 * @brief Saves the list of average colors as a movie barcode PNG image.
 * @param avgColorList A vector of RGBA colors, one for each frame.
 * @param outputPath The path where the final PNG will be saved.
 * @param stripeHeight The desired height of the output image.
 */
auto saveBarcode(
  const std::vector<RGBA>& avgColorList,
  const std::filesystem::path& outputPath,
  unsigned int stripeHeight = 1080) -> void {
  if (avgColorList.empty()) {
    std::println(stderr, "No average colors to save.");
    return;
  }

  unsigned int width = avgColorList.size();
  std::vector<uint8_t> outputPixels(width * stripeHeight * CHANNELS);

  // For each vertical stripe (one for each frame)
  for (int x = 0; x < width; ++x) {
    const auto& color = avgColorList[x];
    // Entire column is that frame's average color
    for (int y = 0; y < stripeHeight; ++y) {
      int index = (y * width + x) * CHANNELS;
      outputPixels[index + 0] = color.r;
      outputPixels[index + 1] = color.g;
      outputPixels[index + 2] = color.b;
      outputPixels[index + 3] = color.a;
    }
  }

  if (stbi_write_png(
    outputPath.c_str(),
    width,
    stripeHeight,
    CHANNELS,
    outputPixels.data(),
    width * CHANNELS)) {
    std::println("Successfully saved barcode to: {}", outputPath.string());
  } else { std::println(stderr, "Failed to save PNG image."); }
}

auto main(int argc, char* argv[]) -> int {
  if (argc < 2) {
    std::println(stderr, "Usage: {} <path_to_movie>", argv[0]);
    return 1;
  }
  const std::filesystem::path moviePath = argv[1];
  if (!std::filesystem::exists(moviePath)) {
    std::println(stderr, "Error: File not found at '{}'", moviePath.string());
    return 1;
  }

  std::println("Getting video dimensions...");
  unsigned int width = 0, height = 0;
  std::string ffprobeCmd = "ffprobe -v error -select_streams v:0 "
                           "-show_entries stream=width,height -of csv=s=x:p=0 '";
  ffprobeCmd += moviePath.string() + "'";

  std::string dims_str;
  FILE* pipe_ffprobe = popen(ffprobeCmd.c_str(), "r");
  if (!pipe_ffprobe) {
    std::println(stderr, "popen() for ffprobe failed!");
    return 1;
  }
  try {
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe_ffprobe) != nullptr) {
      dims_str += buffer.data();
    }
  } catch (...) {
    pclose(pipe_ffprobe);
    throw;
  }
  pclose(pipe_ffprobe);

  if (sscanf(dims_str.c_str(), "%ux%u", &width, &height) != 2 || width == 0 || height == 0) {
    std::println(stderr, "Failed to parse dimensions from ffprobe output: '{}'", dims_str);
    return 1;
  }
  std::println("Got video dimensions: {}x{}", width, height);

  auto pDevice = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  if (!pDevice) {
    std::println(stderr, "Failed to get the default Metal device.");
    return 1;
  }
  auto pCommandQueue = NS::TransferPtr(pDevice->newCommandQueue());

  int pipefd[2];
  if (pipe(pipefd) == -1) { perror("pipe"); return 1; }
  pid_t pid = fork(); // to avoid orphan processes
  if (pid == -1) { perror("fork"); return 1; }
  if (pid == 0) { // child process to run ffmpeg
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO); // redirect stdout to the pipe's write end
    close(pipefd[1]);

    // Replace this process with ffmpeg
    execlp("ffmpeg", "ffmpeg", "-hide_banner", "-loglevel", "error",
           "-i", moviePath.c_str(), "-f", "rawvideo",
           "-pix_fmt", "rgba", "-", nullptr);
    perror("execlp for ffmpeg failed");
    return 1;
  } else {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    std::vector<RGBA> avgColorList;
    const size_t frameSize = width * height * CHANNELS;
    std::vector<uint8_t> frameBuffer(frameSize);
    unsigned int frameCount = 0;
    std::println("Processing frames from ffmpeg stream...");
    try {
      // raw pixel data is ingested from the pipe from ffmpeg
      while (std::cin.read(reinterpret_cast<char*>(frameBuffer.data()), frameSize)) {
        // if (frameCount % 12 == 0) {
        auto pTexture = createTextureFromBuffer(pDevice, frameBuffer, width, height);
        if (!pTexture) {
          std::println(stderr, "Failed to create texture for frame {}", frameCount);
          continue;
        }

        RGBA avgColor = calculateAverageColor(pCommandQueue, pTexture);
        avgColorList.push_back(avgColor);
      // }
        frameCount++;
        if (frameCount % 50 == 0) { // update terminal every 50 frames
          std::println("Processed {} frames...\r", frameCount);
        }
      }
    } catch (const std::exception& e) {
      std::println(stderr, "Error occurred during processing: {}", e.what());
      kill(pid, SIGKILL);
    }

    std::println("\nStream finished. Processed {} frames.", frameCount);
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      std::println(stderr, "ffmpeg exited with non-zero status: {}", WEXITSTATUS(status));
    }
    saveBarcode(avgColorList, "movie_barcode.png");
  }
  // try {
  //   ...
  //   while (std::cin.read(reinterpret_cast<char*>(currentFrameBuffer.data()), frameSize)) {
  //     {
  //       std::lock_guard<std::mutex> lock(queue_mutex);
  //       // Push the index and the frame data as a single work item
  //       frame_queue.push({frame_index, currentFrameBuffer});
  //     }
  //     frame_index++;
  //     queue_cond.notify_one(); // Wake up one waiting worker thread
  //   }
  return 0;
}
