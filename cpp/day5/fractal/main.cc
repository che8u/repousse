#include <atomic>
#include <csignal>
#include <filesystem>
#include <memory>
#include <print>
#include <utility>

#include <simd/simd.h>
#include <mach-o/dyld.h> // For _NSGetExecutablePath

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Metal.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include "fractal_params.hpp"

// template<typename T> using cShr = std::shared_ptr<T>;
// template<typename T> using cUnq = std::unique_ptr<T>;
// template<typename T> using oShr =   NS::SharedPtr<T>;
// template<typename T> using oRet =   NS::RetainPtr<T>;

std::atomic<bool> gIsRunning = true;
auto signalHandler(int signum) -> void{ gIsRunning = false; }

template<typename bufferContent>
struct MetalObjects {
  //  NS::AutoreleasePool* pPool;
  NS::SharedPtr<MTL::Device> pDevice;
  NS::SharedPtr<MTL::Library> pLibrary;
  NS::SharedPtr<MTL::Function> pFunction;
  NS::SharedPtr<MTL::ComputePipelineState> pComputePipelineState;
  NS::SharedPtr<MTL::CommandQueue> pCommandQueue;
  NS::SharedPtr<MTL::Buffer> pBufferContent;

  /**
   * @brief Renders a single frame using Metal as back
   * @param pLayer Metal layer for rendering
   * @param fractalParams Parameters for fractal calculation
   * @details
   *  1. Acquires next drawable from layer
   *  2. Updates GPU buffer with new parameters
   *  3. Encodes compute commands and dispatches threads
   *  4. Presents and commits command buffer
   */
  auto renderFrame (CA::MetalLayer* pLayer, const FractalParams& fractalParams) -> void {
    NS::SharedPtr<CA::MetalDrawable> pDrawable = NS::RetainPtr((pLayer->nextDrawable()));
    if (!pDrawable) [[unlikely]] { return; }

    bufferContent* pGpuParams = static_cast<bufferContent*>(pBufferContent->contents());
    *pGpuParams = fractalParams;
    pBufferContent->didModifyRange(NS::Range(0, sizeof(bufferContent)));

    NS::SharedPtr<MTL::CommandBuffer>   pCommandBuffer = NS::RetainPtr(pCommandQueue->commandBuffer());
    NS::SharedPtr<MTL::ComputeCommandEncoder> pEncoder = NS::RetainPtr(pCommandBuffer->computeCommandEncoder());
    pEncoder->setComputePipelineState(pComputePipelineState.get());
    pEncoder->setTexture(pDrawable->texture(), 0);
    pEncoder->setBuffer(pBufferContent.get(), 0, 0);

    MTL::Size gridSize = MTL::Size(pDrawable->texture()->width(), pDrawable->texture()->height(), 1);

    NS::UInteger threadGroupWidth = pComputePipelineState->threadExecutionWidth();
    NS::UInteger threadGroupHeight = pComputePipelineState->maxTotalThreadsPerThreadgroup() / threadGroupWidth;
    MTL::Size threadgroupSize = MTL::Size(threadGroupWidth, threadGroupHeight, 1);

    pEncoder->dispatchThreads(gridSize, threadgroupSize);
    pEncoder->endEncoding();
    pCommandBuffer->presentDrawable(pDrawable.get());
    pCommandBuffer->commit();
  }

  MetalObjects (
    const std::filesystem::path shaderPath,
    const std::string fnName) {
    //    pPool = NS::AutoreleasePool::alloc()->init();
    pDevice = NS::TransferPtr(MTL::CreateSystemDefaultDevice());

    pLibrary = NS::TransferPtr(pDevice->newLibrary(
      NS::String::string(shaderPath.string().c_str(), NS::StringEncoding::UTF8StringEncoding),
      nullptr));
    if (!pLibrary) {
      throw std::runtime_error("Couldn't find the .metallib file.");
    }

    pFunction = NS::TransferPtr(pLibrary->newFunction(
      NS::String::string(fnName.c_str(), NS::StringEncoding::UTF8StringEncoding)));
    NS::Error* pError = nullptr;
    pComputePipelineState =
      NS::TransferPtr(pDevice->newComputePipelineState(pFunction.get(), &pError));

    if (pError) {
      throw std::runtime_error(pError->localizedDescription()->utf8String());
    } // ! pError lifetime is managed by AutoReleasePool

    pCommandQueue = NS::TransferPtr(pDevice->newCommandQueue());
    pBufferContent = NS::TransferPtr(pDevice->newBuffer(sizeof(bufferContent), MTL::ResourceStorageModeShared));
  }
  //  ~MetalObjects () {}
};

struct WindowGUI {
  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> pWindow;
  FractalParams fractalParams;
  SDL_MetalView metalView;
  bool isDragging;

  /**
   * @brief Processes events and renders frames continuously
   * @note Window may not show up until this function is called. Ref: https://discourse.libsdl.org/t/why-do-you-need-an-event-loop-in-order-for-the-window-to-show-up/
   */
  auto runEventLoop (
    MetalObjects<FractalParams>& metalObjects,
    CA::MetalLayer* pMetalLayer) -> void {
    SDL_Event event;
    while (gIsRunning) {
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_EVENT_QUIT:
            gIsRunning = false; break;

          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) { isDragging = true; }
            break;

          case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) { isDragging = false; }
            break;

          case SDL_EVENT_MOUSE_MOTION:
            if (isDragging) {
              int w, h;
              SDL_GetWindowSize(pWindow.get(), &w, &h);
              // Convert pixel motion to a change in the complex plane's center
              fractalParams.offset.x -= event.motion.xrel * fractalParams.zoom / w;
              fractalParams.offset.y += event.motion.yrel * fractalParams.zoom / w;
              //                                 use width for consistent aspect ▲
            } break;

          case SDL_EVENT_MOUSE_WHEEL: {
            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            int w, h;
            SDL_GetWindowSize(pWindow.get(), &w, &h);

            simd::float2 mousePosComplex = {
              fractalParams.offset.x + (mouseX - w / 2.0f) * fractalParams.zoom / w,
              fractalParams.offset.y - (mouseY - h / 2.0f) * fractalParams.zoom / w
              // ! Y is inverted in screen vs. complex plane
            };

            constexpr float zoomFactor = 0.9f;
            if (event.wheel.y > 0) { fractalParams.zoom *= zoomFactor; }
            else if (event.wheel.y < 0) { fractalParams.zoom /= zoomFactor; }

            fractalParams.offset.x = mousePosComplex.x - (mouseX - w / 2.0f) * fractalParams.zoom / w;
            fractalParams.offset.y = mousePosComplex.y + (mouseY - h / 2.0f) * fractalParams.zoom / w;
            break;
          } // don't break here

          case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE) { gIsRunning = false; } break;
        }
      }
      metalObjects.renderFrame(pMetalLayer, fractalParams);
    }
  }

  WindowGUI(
    const std::string windowTitle,
    const int windowDimX,
    const int windowDimY) : pWindow(nullptr, SDL_DestroyWindow) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    }

    pWindow.reset(SDL_CreateWindow(
      windowTitle.c_str(),
      windowDimX,
      windowDimY,
      SDL_WINDOW_METAL));

    if (!pWindow) { SDL_Log("Failed to create window: %s", SDL_GetError()); }
    SDL_SetWindowResizable(pWindow.get(), false);
    isDragging = false;
  };

  ~WindowGUI() {
    if (metalView) { SDL_Metal_DestroyView(metalView); }
    SDL_Quit();
  };
};

/**
 * @brief Initializes Metal layer for SDL window
 * @param windowGUI Reference to WindowGUI instance
 * @return Pointer to CA::MetalLayer on success, nullptr on failure
 * @throws Logs error via SDL_Log on failure
 */
[[nodiscard("The returned MetalLayer must be configured and used.")]]
auto initMetalLayer(WindowGUI& windowGUI) -> CA::MetalLayer* {
  windowGUI.metalView = SDL_Metal_CreateView(windowGUI.pWindow.get());
  if (!(windowGUI.metalView)) {
    SDL_Log("SDL_Metal_CreateView failed: %s", SDL_GetError());
    return nullptr;
  }

  void* rawLayer = SDL_Metal_GetLayer(windowGUI.metalView);
  if (!rawLayer) {
    SDL_Log("SDL_Metal_GetLayer failed: %s", SDL_GetError());
    return nullptr;
  }

  auto* layer = reinterpret_cast<CA::MetalLayer*>(rawLayer);
  return layer;
}

auto main() -> int {
  signal(SIGINT, signalHandler); // Register the handler

  NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

  WindowGUI windowGUI("Mandelbrot Fractal", 550, 550);
  windowGUI.fractalParams = FractalParams{256, {0.0f, 0.0f}, 4.0f};
  CA::MetalLayer* pMetalLayer = initMetalLayer(windowGUI);
  if (!pMetalLayer) { return -1; }


  auto getResourcePath = [](const std::string& resource_name) -> std::filesystem::path {
    char path_buffer[1024];
    uint32_t size = sizeof(path_buffer);

    if (_NSGetExecutablePath(path_buffer, &size) != 0) {
      throw std::runtime_error("Failed to get executable path.");
    }

    std::filesystem::path exe_path(path_buffer);
    // Final path example ".../Fractal.app/Contents/Resources/[resource_name]"
    return exe_path.parent_path().parent_path() / "Resources" / resource_name;
  };


  std::filesystem::path metal_lib_path = getResourcePath("fractal.metallib");
  MetalObjects<FractalParams> metalObjects(metal_lib_path, "fractal");

  pMetalLayer->setDevice((metalObjects.pDevice).get());
  pMetalLayer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  windowGUI.runEventLoop(metalObjects, pMetalLayer);

  pPool->release();
  return 0;
}
