# Day 3 - Matrix Multiplication

TODO: Tiled matrix multiplication for two 1024x1024 matrices. [Explanation of how tiled matrix-multiplication works](https://alvinwan.com/how-to-tile-matrix-multiplication/#how-does-tiling-work).

## Brief code Explanation

The outer loop (iterating over `tile_idx`) goes through each tile. Within each tile, each thread loads a specific element from `matrix_a` and `matrix_b` into the shared memory tiles `tileA` and `tileB`. This line ensures that all threads load the data before proceeding to the actual calculation:

```C++
threadgroup_barrier(mem_flags::mem_threadgroup);
```

The same logic ensures that computation pertaining to one tile is complete and that we can move to the next tile.

There are two key values to mull over in this problem:
1. Tile Size: It may be tempting to pick larger tiles for fewer iterations, but the key to remember is that our **delimiter is the size of the shared memory**. This is explained well in the aforementioned article.
1. Grid Size: The number of threads per threadgroup as well the number of threadgroups. We use `MTL::Size(16, 16, 1)` for the former. `(MATRIX_DIMENSION + 15) / 16` performs a ceiling division to ensure that we cover the entire matrix. This is however correct only since both our matrices are square. Were it not, it'd be like this:

```C++
MTL::Size numGroups = MTL::Size(
  (cols + 15) / 16,  // Number of threadgroups along the x-dimension (columns)
  (rows + 15) / 16,  // Number of threadgroups along the y-dimension (rows)
  1                  // Number of threadgroups along the z-dimension (depth)
);
```

## Results

I ran the the program with square matrices of dimensions 128x128 and 1024x1024 and got a speedup of 4.2x and 344.2x respectively when using the GPU over the CPU. GPU >> CPU in this regard.

## Crossing the barrier

> This is specific to compute (those utilising [`MTL::ComputeCommandEncoder`](https://developer.apple.com/documentation/metal/mtlcomputecommandencoder)) tasks.

The MSL code this time was vastly different from last time. I had a run-in with `threadgroup_barrier` - a synchronisation primitive. Per the MSL Spec (abridged):
- `threadgroup_barrier` acts as an execution and memory barrier.
- All threads in a threadgroup shall have the same fate. If one thread goes through a conditional, all other threads must do so as well. If one goes through a loop, all other threads follow suit.
- The `threadgroup_barrier` function can also queue a memory fence (for reads and writes) to ensure the correct ordering of memory operations to threadgroup or device memory.



## “Foolish, foolish and old I have become.”

There's an old joke in programming that programmers would rather spend hours automating a task that would've probably taken minutes to do manually [(example)](https://xkcd.com/1319/). So that's where I've been for the last few days.

For starters, I've learnt that I'm not doing Objective-C garbage collection (ARC) correctly. I'm not fully sure as to why this is yet. But here's where I got stuck: It's been a while since I used C-style memory allocation in cpp code. So when I had to manage metal objects like this: `pDevice->release()`, I thought there's gotta be a more elegant way. Objective-C too has smart pointers and so I decided to create a helper class to manage these objects by leveraging `NS::SharedPtr<MTL::Device> pDevice;`.

... and I ran into segfaults. The trace confirms this:

```shell
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BAD_ACCESS (code=1, address=0x1e6aeb1c82f0)
  frame #0: 0x00000001825e4028 libobjc.A.dylib`objc_release + 16
  ...
Target 0: (bin) stopped.
(lldb) bt
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BAD_ACCESS (code=1, address=0x1e6aeb1c82f0)
  * frame #0: 0x00000001825e4028 libobjc.A.dylib`objc_release + 16
    frame #1: 0x00000001825ebc44 libobjc.A.dylib`AutoreleasePoolPage::releaseUntil(objc_object**) + 204
    frame #2: 0x00000001825e8040 libobjc.A.dylib`objc_autoreleasePoolPop + 244
    frame #3: 0x0000000182a80c00 CoreFoundation`_CFAutoreleasePoolPop + 32
    frame #4: 0x0000000184052484 Foundation`-[NSAutoreleasePool release] + 140
    ...
(lldb) quit
```

I'm doing something wrong and I'll have to read some more. Eventually, I just went back to the de-facto way.

Another inevitable thing has happened. I'm going to move onto Xcode for the entire build pipeline. When working with Objective-C objects, a runtime error that just says `Context leak detected, msgtracer returned -1` is not very helpful. Not immediately though, maybe a couple more compute-related programs and then I'll go all in on Xcode.
