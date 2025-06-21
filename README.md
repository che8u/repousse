# repousse

I've drawn some inspiration from the various [100-days-of-CUDA](https://github.com/hkproj/100-days-of-gpu/blob/main/CUDA.md) challenges. Since all I have is a baseline M1 Mac, I'm going to try my own variant where I try to use the Apple Metal API for 100 or so days. 'Repousse' is a technique used to make art of out of metal ([example](https://en.wikipedia.org/wiki/Repouss%C3%A9_and_chasing#/media/File:Mildenhall_treasure_great_dish_british_museum.JPG)). I know, I'm _very_ smart.

These compilation flags must be passed:
- `-fobjc-arc`: to enable Automatic Reference Counting; needed to manage the memory of the underlying Obj-C objects used by Metal
- `framework Foundation`: to link against Foundation framework (`NS::AutoreleasePool` etc.)
- `framework Metal`: to link against the Metal framework

Run this command inside any dir:

```Shell
clang++ -std=c++23 -o bin main.cc -fobjc-arc -framework Foundation -framework Metal
```

Alternatively, every dir has a `makefile`. Using either method will output a `bin` executable. You can skip this step using `make run` to compile and run the code.
