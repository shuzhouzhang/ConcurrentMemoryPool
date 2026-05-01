#include "ConcurrentAlloc.h"
#include <vector>
#include <iostream>

void TestBasicAllocAndFree()
{
    void* p1 = ConcurrentAlloc(6);
    void* p2 = ConcurrentAlloc(8);
    void* p3 = ConcurrentAlloc(1);
    void* p4 = ConcurrentAlloc(7);
    void* p5 = ConcurrentAlloc(8);

    assert(p1 && p2 && p3 && p4 && p5);

    ConcurrentFree(p1, 6);
    ConcurrentFree(p2, 8);
    ConcurrentFree(p3, 1);
    ConcurrentFree(p4, 7);
    ConcurrentFree(p5, 8);
}

void TestSameSizeReuse()
{
    void* p1 = ConcurrentAlloc(8);
    ConcurrentFree(p1, 8);

    void* p2 = ConcurrentAlloc(8);
    assert(p1 == p2);
    ConcurrentFree(p2, 8);
}

void TestBatchAllocAndFree()
{
    std::vector<void*> ptrs;
    ptrs.reserve(200);

    for (size_t i = 0; i < 200; ++i)
    {
        ptrs.push_back(ConcurrentAlloc(64));
        assert(ptrs.back());
    }

    for (void* ptr : ptrs)
    {
        ConcurrentFree(ptr, 64);
    }
}

void WorkerSmallLoop(size_t size, size_t n)
{
    std::vector<void*> ptrs;
    ptrs.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        ptrs.push_back(ConcurrentAlloc(size));
        assert(ptrs.back());
    }

    for (void* ptr : ptrs)
    {
        ConcurrentFree(ptr, size);
    }
}

void TestMultiThread()
{
    std::thread t1(WorkerSmallLoop, 8, 200);
    std::thread t2(WorkerSmallLoop, 16, 200);
    std::thread t3(WorkerSmallLoop, 64, 200);
    t1.join();
    t2.join();
    t3.join();
}

int main()
{
    TestBasicAllocAndFree();
    TestSameSizeReuse();
    TestBatchAllocAndFree();
    TestMultiThread();

    std::cout << "all tests passed" << std::endl;
    return 0;
}
