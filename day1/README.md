# Day 1: Adding Two Vectors

TODO: Add two `std::vector<float>`s; since result of each iteration is independent of the previous (causal independence) - they can be computed in SIMD.

---

## Understanding the Build Process

For compiling the shader, the Xcode bundle needs to be linked. This is a two step process:
1. Link the Xcode bundle to the project: `	xcode-select --switch /Applications/Xcode.app/Contents/Developer`. This requires Xcode to be installed and also why you need to run `sudo make` for this program.
2. Compile the shader: This is again a two-step process. First, running `xcrun -sdk macosx metal -c <something>.metal -o <something>.air` creates a `.air` (Apple Intermediate Representation) file - an intermediate representation. Then, one or more IRs are linked by running `xcrun -sdk macosx metallib <something>.air -o <something>.metallib`. The `.metallib` file is the final output in the Metal build pipeline.

The `.metallib` file is read by the code at runtime.

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

So I reran the program on a stronger machine - M3 Pro with 18Gig of memory:

| Benchmark              | Time            | CPU             | Iterations |
|-----------------------|-----------------|-----------------|------------|
| BM_CPU/100000000      | 9903958917 ns   | 9868660000 ns   | 1          |
| BM_CPU/200000000      | 1.9523e+10 ns   | 1.9491e+10 ns   | 1          |
| BM_Metal/100000000    | 9591659916 ns   | 9466587000 ns   | 1          |
| BM_Metal/200000000    | 2.0187e+10 ns   | 1.9418e+10 ns   | 1          |

The difference is still not clear. My guess is that since I'm using `float`s here, the program itself has become memory-bound. I can only confirm this by profiling the GPU (perhaps using Xcode Instruments) but I'm yet to learn how to do that.
