#pragma once
#include "Common.h"

// 1. 前向声明ThreadCache，避免循环依赖
class ThreadCache;

// 2. 正确声明TLS变量：extern + __declspec(thread) + 完整类型
extern __declspec(thread) ThreadCache* pTLSThreadCache;

class ThreadCache
{
public:
    // 申请空间
    void* Allocate(size_t size);
    // 释放空间（修正为void，符合语义）
    void Deallocate(void* ptr, size_t size);
    // 从中心缓存申请
    void* FetchFromCentralCache(size_t index, size_t size);
    // freelist过长时回收一部分对象到central cache
    void ListTooLong(FreeList& list, size_t size);

private:
    // 成员变量加下划线，规范命名
    FreeList _FreeLists[NFREE_LIST];
};
