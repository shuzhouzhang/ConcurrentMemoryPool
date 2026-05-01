#pragma once
#include"Common.h"
#include<unordered_map>

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	Span* MapObjectToSpan(void* obj);
	void NewSpanMap(Span* span);
	//获取k页的span
	Span* NewSpan(size_t k);
	//回收span到page cache
	void ReleaseSpanToPageCache(Span* span);

	
mutex _pageMtx;
private:
	PageCache() 
	{}
	PageCache(const PageCache&) = delete;

	SpanList _spanLists[NPAGES];
	unordered_map<PAGE_ID, Span*> _idSpanMap;
	
	static PageCache _sInst;
};
