#pragma once
#include <cstddef> // 包含size_t
#include<assert.h>
#include<algorithm>

#include<thread>
#include<mutex>
using namespace std;
#ifdef _WIN32
#include<Windows.h>
#else
//
#endif
static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREE_LIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif defined(_WIN32)
typedef size_t PAGE_ID;
#else
typedef size_t PAGE_ID;
#endif


inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // linux下brk mmap等
#endif

    if (ptr == nullptr)
        throw std::bad_alloc();

    return ptr;
}

static void*& NextObj(void* obj)
{
    return *(void**)obj;
}

class FreeList
{
private:
    // 1. 私有成员函数：必须在类内、Push之前声明，确保编译器可见
  
    // 2. 私有成员变量：显式初始化，避免野指针，解决未定义问题
    void* _freelist = nullptr;
    size_t _maxsize = 1;
    size_t _size = 0;

public:
 

    void PushRange(void* start, void* end)
    {
        if (start == nullptr || end == nullptr) return;
        NextObj(end) = _freelist;
        _freelist = start;

        size_t n = 1;
        void* cur = start;
        while (cur != end)
        {
            cur = NextObj(cur);
            ++n;
        }
        _size += n;
    }
    // 3. Push函数：正确访问类成员_freelist
    void Push(void* obj)
    {
        if (obj == nullptr) return; // 空指针防护

        NextObj(obj) = _freelist; // 现在NextObj是类成员，能正确访问_freelist
        _freelist = obj; // 必须更新链表头，之前漏了这行！
        ++_size;
    }

    // 4. Pop函数：必须返回取出的节点，不能是void
    void* Pop()
    {
        if (_freelist == nullptr) return nullptr;

        void* obj = _freelist;
        _freelist = NextObj(_freelist);
        --_size;
        return obj;
    }
    void PopRange(void*& start, void*& end, size_t n)
    {
        assert(n > 0);

        start = _freelist;
        end = start;
        if (start == nullptr)
        {
            end = nullptr;
            return;
        }

        size_t i = 1;
        while (i < n && NextObj(end) != nullptr)
        {
            end = NextObj(end);
            ++i;
        }

        _freelist = NextObj(end);
        NextObj(end) = nullptr;
        _size -= i;
    }

    // 5. 判空函数
    bool Empty() const
    {
        return _freelist == nullptr;
    }
    size_t& MaxSize()
    {
        return _maxsize;
    }
    size_t Size() const
    {
        return _size;
    }

};


//计算对象大小的对齐规则
class SizeClass
{
public:
    //整体控制在10%内碎片浪费
  static inline  size_t _Roundup(size_t size, size_t AlignNum)
    {

        size_t alignSize;
        if (size % AlignNum != 0)
        {
            alignSize = (size / AlignNum + 1) * AlignNum;
        }
        else
        {
            alignSize = size;
        }
        return alignSize;
    }
  static inline size_t Roundup(size_t size)
    {
        if (size <= 128)
        {
            return   _Roundup(size, 8);
        }
        else if (size <= 1024)
        {
          return   _Roundup(size, 16);

        }
        else if (size <= 8 * 1024)
        {
            return  _Roundup(size, 128);

        }
        else if (size <= 64 * 1024)
        {
            return  _Roundup(size, 1024);

        }
        else if (size <= 256 * 1024)
        {
            return   _Roundup(size, 8 * 1024);

        }
        else
        {
            assert(false);
            return -1;
        }
    }

 static inline size_t _Index(size_t bytes,size_t alignNum)
  {
      if (bytes % alignNum == 0)
      {
          return bytes / alignNum - 1;
      }
      else
      {
          return bytes / alignNum;
      }
  }

 static inline size_t Index(size_t bytes)
 {
     assert(bytes <= 256 * 1024);

     static int group_array[4] = { 16, 56, 56, 56 };

     if (bytes <= 128)
     {
         return _Index(bytes, 8);
     }
     else if (bytes <= 1024)
     {
         return _Index(bytes - 128, 16) + group_array[0];
     }
     else if (bytes <= 8 * 1024)
     {
         return _Index(bytes - 1024, 128) + group_array[0] + group_array[1];
     }
     else if (bytes <= 64 * 1024)
     {
         return _Index(bytes - 8 * 1024, 1024) + group_array[0] + group_array[1] + group_array[2];
     }
     else if (bytes <= 256 * 1024)
     {
         return _Index(bytes - 64 * 1024, 8 * 1024) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
     }

     // ✅ 补充缺失的返回值
     assert(false);
     return -1;
 
 }
 static size_t NumMoveSize(size_t size)
 {
     assert(size > 0);
     size_t num = MAX_BYTES / size;
     if (num < 2)
         num = 2;
     else if (num > 512)
         num = 512;
     return num;

 }

 //计算一次向系统要多少个页
 static size_t NumMovePage(size_t size)
 {
     size_t num = NumMoveSize(size);
     size_t npage = num * size;
     npage >>= PAGE_SHIFT;
     if (npage == 0)
         npage = 1;
     return npage;
 }

  

};
//管理大块内存的连续跨度
struct Span
{
    size_t _PageID = 0;//页号
    size_t _n = 0;//页数

    Span* _next = nullptr;
    Span* _prev = nullptr;

    size_t _useCount = 0;//切好小块内存被分配给threadcachae的记数
    void* _freeList = nullptr;//切好的小块内存的自由链表

};
//带头双向循环链表
class SpanList
{
public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }
    Span* Begin()
    {
        return _head->_next;
    }
    Span* End()
    {
        return _head;
    }
    bool Empty()
    {
        return _head->_next == _head;
    }
    void PushFront(Span* span)
    {
        Insert(Begin(), span);
    }
    void Insert(Span* pos, Span* newSpan)
    {
        assert(pos);
        assert(newSpan);
        Span* prev = pos->_prev;
        prev->_next = newSpan;
        newSpan->_prev = prev;
        newSpan->_next = pos;
        pos->_prev = newSpan;
    }
    Span* PopFront()
    {
        // 【关键】空链表防护：如果链表为空，直接返回nullptr，绝不删除头节点
        if (_head->_next == _head) 
        {
            return nullptr;
        }
        Span* front = _head->_next;
        Erase(front);
        return front;
    }
    void Erase(Span* pos)
    {
        // 1. 加强入参校验
        assert(pos != nullptr);
        assert(pos != _head);

        // 2. 保存前后节点指针
        Span* prev = pos->_prev;
        Span* next = pos->_next;

        // 3. 修复链表指针
        prev->_next = next;
        next->_prev = prev;

        // 4. 【关键】将被删除节点的指针置空，防止野指针复用
        pos->_prev = nullptr;
        pos->_next = nullptr;
    }
private:
    Span* _head;
public:
    mutex _mtx;//桶锁

};
