#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	//����256kb���ڴ�����Ҫ��
	if (size > MAX_BYTES)
	{
		//����������Ҫ�����ҳ��
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kPage = alignSize >> PAGE_SHIFT;

		//��page cache����kPageҳ��span
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//ͨ��TLS��ÿ���߳�������ȡ�Լ�ר����thread cache����
		if (pTLSThreadCache == nullptr)
		{
			static std::mutex tcMtx;
			static ObjectPool<ThreadCache> tcPool;
			tcMtx.lock();
			pTLSThreadCache = tcPool.New();
			tcMtx.unlock();
		}

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	//����256kb���ڴ��ͷ�
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}