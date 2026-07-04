#pragma once
#include"Common.h"
#include"ThreadCache.h"
static void* ConcurrentAlloc(size_t size)
{
	if (size == 0)
	{
		size = 1;
	}
	if (size > MAX_BYTES)
	{
		return std::malloc(size);
	}
	if (pTLSThreadCache == nullptr)
	{
		static thread_local ThreadCache tlsThreadCache;
		pTLSThreadCache = &tlsThreadCache;
	}
	return pTLSThreadCache->Allocate(size);
}
static void ConcurrentFree(void* ptr, size_t size)
{
	if (ptr == nullptr)
	{
		return;
	}
	if (size == 0)
	{
		size = 1;
	}
	if (size > MAX_BYTES)
	{
		std::free(ptr);
		return;
	}
	assert(pTLSThreadCache);
	pTLSThreadCache->Deallocate(ptr, size);

}
