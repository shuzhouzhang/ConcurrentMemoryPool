#include"ThreadCache.h"
#include"CentralCache.h"
__declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t aligbsize = SizeClass::Roundup(size);
	size_t index = SizeClass::Index(size);
	if (!_FreeLists[index].Empty())
	{
		return _FreeLists[index].Pop();

	}
	else
	{
		return FetchFromCentralCache(index, aligbsize);
	}

}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr != nullptr);
	assert(size <= MAX_BYTES);

	size_t alignSize = SizeClass::Roundup(size);
	size_t index = SizeClass::Index(size);
	// 先还给当前线程自己的freelist，尽量在本线程内完成释放
	_FreeLists[index].Push(ptr);

	// freelist挂得太长时，再批量吐给central cache
	//如果大于一次申请的最大内存就返还
	if (_FreeLists[index].Size()>=_FreeLists[index].MaxSize())
	{
		ListTooLong(_FreeLists[index], alignSize);
	}
}
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//慢增长
	size_t batchNum = min(SizeClass::NumMoveSize(size), _FreeLists[index].MaxSize());
	if (batchNum == _FreeLists[index].MaxSize())
		_FreeLists[index].MaxSize() += 1;
	void* start = nullptr;
	void* end = nullptr;
	
	
	size_t actualNum =CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		_FreeLists[index].PushRange(NextObj(start), end);
		return start;
	}


	
	return nullptr;

}
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	// 一次回收一段，避免freelist无限增长
	size_t batchNum = min(list.Size(), SizeClass::NumMoveSize(size));
	list.PopRange(start, end, batchNum);
	// 交给central cache，由它把这些对象重新挂回各自所属的span
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);

}


