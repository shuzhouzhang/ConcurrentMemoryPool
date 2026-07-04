# ConcurrentMemoryPool

一个用于 C++ Linux 后端实习展示的高并发内存池项目。项目目标不是实现生产级 TCMalloc，而是用较小代码量说明小对象内存复用、线程本地缓存、中心缓存、页缓存、自由链表和锁竞争优化的基本思路。

## 技术栈

- C++11
- Linux / GCC / Make
- pthread / `std::thread`
- `thread_local`
- AddressSanitizer 可选

## 目录结构

```text
.
├── Common.h             # 公共常量、SizeClass、FreeList、Span、SpanList
├── ConcurrentAlloc.h    # 对外申请/释放接口
├── ThreadCache.h/.cpp   # 线程本地小对象缓存
├── CentralCache.h/.cpp  # 多线程共享的中心缓存
├── PageCache.h/.cpp     # 页级 span 管理
├── uniTest.cpp          # 功能正确性测试
├── benchmark.cpp        # malloc/free 与内存池粗略 benchmark
└── Makefile             # Linux/GCC 构建入口
```

## 架构

```text
Application
   |
ConcurrentAlloc / ConcurrentFree
   |
ThreadCache
   |
CentralCache
   |
PageCache
   |
System malloc / page allocation
```

## 构建环境

推荐环境：

- Linux 或 WSL
- `g++` 支持 C++11
- `make`

编译：

```bash
make clean
make
```

## 测试

运行功能测试：

```bash
make test
```

看到如下输出表示当前测试通过：

```text
all tests passed
```

当前测试覆盖：

- 单线程多种 size 申请、写入、释放
- 边界 size：1、8、16、32、64、128、256、512、1024、接近小对象阈值、最大小对象阈值
- 0 字节申请，当前按 1 字节处理
- 大于 256KB 的申请，当前走 `malloc/free`
- 多线程同时申请和释放
- 批量申请、打乱释放、再次申请验证复用路径

## Benchmark

运行粗略性能对比：

```bash
make bench
```

benchmark 比较：

- `malloc/free`
- `ConcurrentAlloc/ConcurrentFree`
- 单线程不同 size 多轮申请释放
- 2 线程、4 线程并发申请释放

输出示例：

```text
malloc/free: xxx ms
ConcurrentMemoryPool: xxx ms
```

注意：该 benchmark 只是本地粗略对比，结果会受 CPU、系统负载、编译选项、WSL/原生 Linux 环境影响。不要在简历里写固定性能提升百分比，除非保留了真实可复现的测试环境和结果。

## ASAN 和 Valgrind

如果 GCC 支持 AddressSanitizer，可以运行：

```bash
make asan-test
```

如果本机安装了 Valgrind，可以运行：

```bash
valgrind --leak-check=full ./build/cmp_test
```

## 申请流程

1. `ConcurrentAlloc(size)` 处理 0 字节和大对象。
2. 小对象进入当前线程的 `ThreadCache`。
3. `SizeClass` 将 size 向上对齐，并映射到自由链表下标。
4. 当前线程对应自由链表非空时直接弹出对象，无需加共享锁。
5. 当前线程自由链表为空时，向 `CentralCache` 批量申请对象。
6. `CentralCache` 没有可用对象时，向 `PageCache` 申请 span，并切分成小对象。

## 释放流程

1. `ConcurrentFree(ptr, size)` 根据 size 判断大对象或小对象。
2. 大对象直接交给 `free`。
3. 小对象先归还到当前线程的 `ThreadCache` 自由链表。
4. 当线程本地自由链表过长时，批量归还给 `CentralCache`。
5. `CentralCache` 根据页号找到所属 span，更新 span 的自由链表和使用计数。
6. span 上的小对象全部归还后，span 会回到 `PageCache`，并尝试与相邻空闲 span 合并。

## 核心模块职责

`ThreadCache`：每个线程独立持有，负责小对象的快速申请和释放。多数命中场景不需要访问共享结构，因此能减少锁竞争。

`CentralCache`：按 size class 管理共享 span，每个桶有独立互斥锁。线程本地缓存不足或过长时，才批量和中心缓存交互。

`PageCache`：负责页级 span 的申请、切分、回收和合并。页号到 span 的映射由 PageCache 维护，并通过页级互斥锁保护。

`SizeClass`：负责 size 对齐、桶下标计算、批量移动数量和页数计算。

`FreeList`：单向自由链表，用空闲对象自身前几个字节保存 next 指针。

## 为什么能减少锁竞争

普通 `malloc/free` 在多线程下可能频繁进入共享分配器路径。本项目把小对象缓存放在线程本地，申请和释放优先在本线程自由链表完成。只有本地缓存为空或过长时，才批量访问 `CentralCache`，从而减少共享锁的进入次数。`CentralCache` 又按 size class 分桶加锁，避免所有小对象竞争同一把锁。

## 当前限制

- 这是学习和面试展示项目，不是生产级内存分配器。
- 小对象最大支持到 `256KB`。
- 大于 `256KB` 的对象当前直接走 `malloc/free`。
- `ConcurrentFree` 需要调用方传入和申请时一致的 size。
- 当前测试覆盖多线程同线程申请释放；跨线程释放未作为主要支持场景充分验证。
- 没有实现系统页释放回操作系统，PageCache 持有的内存会用于后续复用。
- 没有实现完整 TCMalloc 的 thread cache 回收策略、采样、统计和调优能力。

## 后续优化方向

- 增加跨线程释放的专项测试和设计说明。
- 为 PageCache 增加更完整的 span 生命周期检查。
- 增加更稳定的 benchmark 配置和结果记录脚本。
- 增加统计接口，观察不同 size class 的命中率和回收情况。
- 使用 RAII 锁替换手写 lock/unlock，降低异常或早退路径风险。

## 面试可讲点

- 为什么使用 `thread_local` 保存 `ThreadCache`，以及它如何减少共享锁竞争。
- size class 如何通过对齐控制内部碎片，并映射到固定数量的自由链表。
- `CentralCache` 为什么按桶加锁，而不是全局一把锁。
- span 如何从 `PageCache` 申请、切分、回收和合并。
- 当前项目为什么不是生产级分配器，以及还有哪些边界场景需要继续验证。
