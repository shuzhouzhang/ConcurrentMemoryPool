#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;

//获取一个非空的span
Span* CentralCache:: GetOneSpan(SpanList& list, size_t size)
{
	//查看当前spanlist是否还有空闲的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		else
		{
			it = it->_next;
		}
		
	}
	//先把centralCache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
	list._mtx.unlock();
	//说明spanlist里没有空闲的span只能找下一层要
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();
	list._mtx.lock();
	//对span进行切分，不需要加锁
	//span的起始地址和大块内存数
	char* start = (char*)(span->_PageID << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;
	//把大块内存切成自由链表挂起来
	//先切一块下来做头
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	//切好span以后把span挂到桶里面
	list.PushFront(span);
	return span;
}

size_t CentralCache:: FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{

	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);
	//从span中获取batchNum个对象
	//如果不够有多少拿多少
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while( i < batchNum - 1&&NextObj(end)!=nullptr)
	{
		end = NextObj(end);
		i++;
		actualNum++;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;
	_spanLists[index]._mtx.unlock();

	return  actualNum;
	

}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	while (start)
	{
		void* next = NextObj(start);

		// 通过页号映射直接找到对象所属的span，不再遍历整个桶
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// 头插回span自己的自由链表，并同步减少已分配计数
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		--span->_useCount;

		// 这个span上的对象已经全部归还，可以继续还给page cache
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}

	_spanLists[index]._mtx.unlock();
}

