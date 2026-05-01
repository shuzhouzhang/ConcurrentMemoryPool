#include"PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
	auto it = _idSpanMap.find(id);
	assert(it != _idSpanMap.end());
	return it->second;
}

void PageCache::NewSpanMap(Span* span)
{
	assert(span);
	PAGE_ID begin = span->_PageID;
	PAGE_ID end = begin + span->_n;
	for (PAGE_ID i = begin; i < end; ++i)
	{
		_idSpanMap[i] = span;
	}
}

Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0 && k < NPAGES);

	if (!_spanLists[k].Empty())
	{
		return _spanLists[k].PopFront();
	}

	// Check larger page buckets and split a k-page span from them.
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nspan = _spanLists[i].PopFront();
			Span* kspan = new Span;

			// Split k pages from the front of nspan.
			kspan->_PageID = nspan->_PageID;
			kspan->_n = k;
			kspan->_freeList = nullptr;
			kspan->_useCount = 0;

			nspan->_n -= k;
			nspan->_PageID += k;
			nspan->_freeList = nullptr;
			nspan->_useCount = 0;

			NewSpanMap(kspan);
			NewSpanMap(nspan);
			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}

	// No larger span is available, request a big span from system first.
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_PageID = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	NewSpanMap(bigSpan);
	_spanLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	assert(span);

	// Restore span to page-level free state, then try to merge neighbors.
	span->_freeList = nullptr;
	span->_useCount = 0;

	while (span->_PageID > 0)
	{
		PAGE_ID prevId = span->_PageID - 1;
		auto prevIt = _idSpanMap.find(prevId);
		if (prevIt == _idSpanMap.end())
			break;

		Span* prevSpan = prevIt->second;
		if (prevSpan == span)
			break;

		if (prevSpan->_useCount != 0 || prevSpan->_freeList != nullptr)
			break;

		if (prevSpan->_PageID + prevSpan->_n != span->_PageID)
			break;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		prevSpan->_n += span->_n;
		span = prevSpan;
		NewSpanMap(span);
	}

	while (true)
	{
		PAGE_ID nextId = span->_PageID + span->_n;
		auto nextIt = _idSpanMap.find(nextId);
		if (nextIt == _idSpanMap.end())
			break;

		Span* nextSpan = nextIt->second;
		if (nextSpan == span)
			break;

		if (nextSpan->_useCount != 0 || nextSpan->_freeList != nullptr)
			break;

		if (span->_PageID + span->_n != nextSpan->_PageID)
			break;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		span->_n += nextSpan->_n;
		NewSpanMap(span);
	}

	NewSpanMap(span);
	_spanLists[span->_n].PushFront(span);
}
