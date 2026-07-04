#include "ConcurrentAlloc.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
volatile uintptr_t g_sink = 0;

double ToMs(Clock::duration d)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count() / 1000.0;
}

template <class Alloc, class Free>
long long RunSingleThread(const std::vector<size_t>& sizes, size_t iterations, Alloc alloc, Free freeFn)
{
    auto begin = Clock::now();
    for (size_t i = 0; i < iterations; ++i)
    {
        for (size_t size : sizes)
        {
            void* ptr = alloc(size);
            std::memset(ptr, static_cast<int>(i), size);
            g_sink ^= reinterpret_cast<uintptr_t>(ptr);
            g_sink += static_cast<unsigned char*>(ptr)[0];
            freeFn(ptr, size);
        }
    }
    return ToMs(Clock::now() - begin);
}

template <class Alloc, class Free>
long long RunMultiThread(size_t threadCount, const std::vector<size_t>& sizes, size_t iterations, Alloc alloc, Free freeFn)
{
    auto begin = Clock::now();
    std::vector<std::thread> threads;
    for (size_t t = 0; t < threadCount; ++t)
    {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < iterations; ++i)
            {
                for (size_t size : sizes)
                {
                    void* ptr = alloc(size);
                    std::memset(ptr, static_cast<int>(i + t), size);
                    g_sink ^= reinterpret_cast<uintptr_t>(ptr);
                    g_sink += static_cast<unsigned char*>(ptr)[0];
                    freeFn(ptr, size);
                }
            }
        });
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }
    return ToMs(Clock::now() - begin);
}
}

int main()
{
    const std::vector<size_t> sizes = { 8, 64, 256, 1024 };
    const size_t singleIterations = 100000;
    const size_t multiIterations = 50000;

    auto mallocAlloc = [](size_t size) { return std::malloc(size); };
    auto mallocFree = [](void* ptr, size_t) { std::free(ptr); };
    auto poolAlloc = [](size_t size) { return ConcurrentAlloc(size); };
    auto poolFree = [](void* ptr, size_t size) { ConcurrentFree(ptr, size); };

    std::cout << "single thread sizes: 8, 64, 256, 1024 bytes" << std::endl;
    std::cout << "malloc/free: " << RunSingleThread(sizes, singleIterations, mallocAlloc, mallocFree) << " ms" << std::endl;
    std::cout << "ConcurrentMemoryPool: " << RunSingleThread(sizes, singleIterations, poolAlloc, poolFree) << " ms" << std::endl;

    for (size_t threadCount : { 2ul, 4ul })
    {
        std::cout << threadCount << " threads sizes: 8, 64, 256, 1024 bytes" << std::endl;
        std::cout << "malloc/free: " << RunMultiThread(threadCount, sizes, multiIterations, mallocAlloc, mallocFree) << " ms" << std::endl;
        std::cout << "ConcurrentMemoryPool: " << RunMultiThread(threadCount, sizes, multiIterations, poolAlloc, poolFree) << " ms" << std::endl;
    }

    return 0;
}
