# Embedded C++ Buddy Allocator

## David B. Robins

Copyright &copy;2015 David B. Robins. Released under the Modified (3-clause) BSD License.

First release 2015-01-23.

### Overview

This is a [buddy allocator](http://en.wikipedia.org/wiki/Buddy_memory_allocation) written in C++ (C++11) for use in an embedded environment.

It was originally used on an ARM software and has been built with GCC 4.8 and 4.9 (specifically, [GNU tools for ARM embedded processors](https://launchpad.net/gcc-arm-embedded)). Permission has been given by my employer to redistribute it provided the company name is not included. As of first release, we have been using it for about three months in several hundred devices and consider it stable production code. I wrote it because I wanted more deterministic fragmentation behavior than I could get from the default first-fit allocator.

### Building

The project includes an SConstruct file that will build with the [SCons](http://www.scons.org/) software construction tool. It is set up to build a test harness using [Googletest](https://code.google.com/p/googletest/) (set environment variable `GTEST_DIR` to your Googletest path, e.g. `C:\tools\gtest\gtest-1.7.0`) using MinGW on Windows (while the allocator was written for an embedded system, the test project as a whole requires a host; there's no reason it shouldn't run on Linux or other OSes with minor changes). Running `scons` will build an executable test harness that can be run as `build/test`.

### Use

Memory overhead is 2 bits per smallest block, i.e., if the smallest block is 16 (2^4) bytes and the arena size is 1k (2^10), then it would be 2*1024/16 or 128 bits = 16 bytes.

Create an arena by including the header and defining an `Allocator<min, heap>` where `min` is the size of the smallest block as a power of 2 (e.g., use 4 for 16) and `heap` is the entire heap size, also as a power of 2 (e.g., use 10 for 1024 or 1k).

    #include "Alloc.h"

    Allocator<4, 10> g_alloc;
 
Memory can be allocated using the `PtAlloc` methods; there is one for an array allocation, which takes a length, and another for allocating a regular type. They return a `std::unique_ptr<T>` (or `T[]`) which will free the memory when it goes out of scope.

    auto pmystruct = g_alloc.PtAlloc<MyStruct>();

    auto pb = g_alloc.PtAlloc<uint8_t>(32);

If the allocation fails, `nullptr` is returned (exceptions were not enabled in the embedded project).

If it is necessary to detach the pointer without freeing it, use `std::unique_ptr<T>::release`; you can re-attach it to obtain another `std::unique_ptr<T>` with the appropriate deleter using `PtAttach` (or `PtAttachRg` for arrays). Prefer using and moving the `std::unique_ptr<T>` if possible.

    auto pfoo = g_alloc.PtAlloc<Foo>();

    ABC abc;
    abc.pfoo = pfoo.release();  // ABC::pfoo is a Foo *

    // ... later ...

    auto pfoo = g_alloc.PtAttach(abc.pfoo);

### Methods for Legacy Code 

If it better matches your project's requirements, you can expose the private `PvAlloc` and `FreePv` methods and use them to allocate bytes and return a `void *` and free directly rather than using RAII. This is not recommended except for use with legacy code.

### Implementation

The implementation sacrifices some speed to save memory by not using free lists; instead, a bitmap representing a contiguous array of the smallest block is used to track allocations, with 2 bits per smallest block (see above). The first (high) bit is set if the block is in-use; the second is set if it is the end of a logical block. For example, the initial state is 00 00 00 ... 00 01, meaning one free block the size of the whole arena. It can then be split recursively down to the minimum size by flagging the appropriate smallest-block as an end-block.   

It may be instructive to define `TEST_OUTPUT` when running the tests to see how the block bitmap changes after various operations.
