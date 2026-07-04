#include "ConcurrentAlloc.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace
{
void FillAndCheck(void* ptr, size_t size, unsigned char value)
{
    assert(ptr != nullptr);
    size_t n = size == 0 ? 1 : size;
    std::memset(ptr, value, n);

    unsigned char* bytes = static_cast<unsigned char*>(ptr);
    for (size_t i = 0; i < n; ++i)
    {
        assert(bytes[i] == value);
    }
}

void TestBasicAllocAndFree()
{
    std::vector<size_t> sizes = { 1, 6, 8, 15, 24, 63, 127, 129, 511, 1023, 4096 };
    for (size_t size : sizes)
    {
        void* ptr = ConcurrentAlloc(size);
        FillAndCheck(ptr, size, static_cast<unsigned char>(size % 251));
        ConcurrentFree(ptr, size);
    }

    void* zero = ConcurrentAlloc(0);
    FillAndCheck(zero, 1, 0xAB);
    ConcurrentFree(zero, 0);
}

void TestBoundarySizes()
{
    std::vector<size_t> sizes = {
        1, 8, 16, 32, 64, 128, 256, 512, 1024,
        MAX_BYTES - 8192, MAX_BYTES
    };

    for (size_t size : sizes)
    {
        void* ptr = ConcurrentAlloc(size);
        FillAndCheck(ptr, size, 0x5A);
        assert(reinterpret_cast<size_t>(ptr) % alignof(void*) == 0);
        ConcurrentFree(ptr, size);
    }
}

void TestLargeObjectFallback()
{
    size_t size = MAX_BYTES + 1024;
    void* ptr = ConcurrentAlloc(size);
    FillAndCheck(ptr, size, 0x33);
    ConcurrentFree(ptr, size);
}

void TestSameSizeReuse()
{
    void* p1 = ConcurrentAlloc(64);
    ConcurrentFree(p1, 64);

    void* p2 = ConcurrentAlloc(64);
    assert(p1 == p2);
    ConcurrentFree(p2, 64);
}

void TestBatchAllocShuffleFreeAndReuse()
{
    const size_t size = 64;
    std::vector<void*> ptrs;
    ptrs.reserve(512);

    for (size_t i = 0; i < 512; ++i)
    {
        void* ptr = ConcurrentAlloc(size);
        FillAndCheck(ptr, size, static_cast<unsigned char>(i % 251));
        ptrs.push_back(ptr);
    }

    std::mt19937 rng(20260704);
    std::shuffle(ptrs.begin(), ptrs.end(), rng);

    for (void* ptr : ptrs)
    {
        ConcurrentFree(ptr, size);
    }

    std::vector<void*> reused;
    reused.reserve(128);
    for (size_t i = 0; i < 128; ++i)
    {
        void* ptr = ConcurrentAlloc(size);
        assert(ptr != nullptr);
        reused.push_back(ptr);
    }

    size_t reuseCount = 0;
    for (void* ptr : reused)
    {
        if (std::find(ptrs.begin(), ptrs.end(), ptr) != ptrs.end())
        {
            ++reuseCount;
        }
        ConcurrentFree(ptr, size);
    }
    assert(reuseCount > 0);
}

void WorkerLoop(size_t threadId, size_t rounds)
{
    std::vector<size_t> sizes = { 8, 16, 32, 64, 128, 256, 512, 1024, 4096 };
    for (size_t round = 0; round < rounds; ++round)
    {
        std::vector<void*> ptrs;
        ptrs.reserve(sizes.size());

        for (size_t size : sizes)
        {
            void* ptr = ConcurrentAlloc(size);
            FillAndCheck(ptr, size, static_cast<unsigned char>((threadId + round + size) % 251));
            ptrs.push_back(ptr);
        }

        for (size_t i = ptrs.size(); i > 0; --i)
        {
            ConcurrentFree(ptrs[i - 1], sizes[i - 1]);
        }
    }
}

void TestMultiThread()
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 4; ++i)
    {
        threads.emplace_back(WorkerLoop, i, 1000);
    }

    for (std::thread& t : threads)
    {
        t.join();
    }
}
}

int main()
{
    TestBasicAllocAndFree();
    TestBoundarySizes();
    TestLargeObjectFallback();
    TestSameSizeReuse();
    TestBatchAllocShuffleFreeAndReuse();
    TestMultiThread();

    std::cout << "all tests passed" << std::endl;
    return 0;
}
