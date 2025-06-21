# Day 1: Introduction to Metal

<!-- Learnings:
- Error Handling occurs at <>. There are two key environment variables required for catching errors at comptime ([source](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage/#Enable-API-Validation-with-environment-variables)):
  - `MTL_DEBUG_LAYER=1`
  - `MTL_SHADER_VALIDATION=1` -->

---

### Shader

I initially wrote the MSL code in `add_vec.metal` using `std::string` and then an error hit me.


---

### What exactly is a data buffer?
We [created a data buffer](https://developer.apple.com/documentation/metal/performing-calculations-on-a-gpu?language=objc#Create-Data-Buffers-and-Load-Data) - this is to load the data into the GPU's memory.

Ig it's similar to prefetching - but I'll have to check on that.

Now when you use `MTL::ResourceStorageModeShared`, the buffer is shared between the CPU and the GPU. Therefore, there's no need to copy data between them. See [other paradigms](https://developer.apple.com/documentation/metal/mtlstoragemode?language=objc#Storage-Mode-Options).

---

### Results

My M1 Mac (with 8 Gigs of memory) started to tap out as I approached 2e8 elements. And the results were pretty inconsistent.

| Benchmark            | Time (ns)      | CPU (ns)      | Iterations |
|---------------------|---------------|--------------|------------|
| BM_CPU/100000000    | 1.2876e+10    | 1.2803e+10   | 1          |
| BM_CPU/200000000    | 2.5761e+10    | 2.5589e+10   | 1          |
| BM_Metal/100000000  | 1.3022e+10    | 1.2469e+10   | 1          |
| BM_Metal/200000000  | 2.6355e+10    | 2.5305e+10   | 1          |

So I'm gonna rerun the program on a stronger machine -->
