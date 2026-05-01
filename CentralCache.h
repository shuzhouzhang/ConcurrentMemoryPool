#pragma once
#include"Common.h"
//单例模式
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	//从中心缓存获取批量内存
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	//从SpanList或者PageCache获取Span
	Span* GetOneSpan(SpanList& list, size_t size);
	//将thread cache过长的链表还给central cache
	void ReleaseListToSpans(void* start, size_t size);
private:
	CentralCache()
	{ }
	CentralCache(const CentralCache&) = delete;


	SpanList _spanLists[NFREE_LIST];
	static CentralCache _sInst;
};
