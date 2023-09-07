#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0 && k < NPAGES);
	if (k > NPAGES - 1)    //大于128页直接找堆进行申请
	{
		void* ptr = SystemAlloc(k);
		Span* span = new Span;
		//Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;

		//建立页号与span之间的映射
		_idSpanMap[span->_pageId] = span;
		//_idSpanMap.set(span->_pageId, span);

		return span;
	}

	//然后检查第k个桶里有没有空闲span
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();    //从头部取出

		//建立页号与span的映射，方便central cache回收小块内存时
		//查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			_idSpanMap[kSpan->_pageId + i] = kSpan;
			//_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}

	//检查后面的桶是否有span，如果有可以将其进行拆分
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = new Span;
			//Span* kSpan = _spanPool.New();

			//在nSpan的头部分割对应页数
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;

			//将剩下的挂到对应的映射位置
			_spanLists[nSpan->_n].PushFront(nSpan);
			//存储nSpan的首尾页号与nSpan之间的映射，
			//方便page cache合并span时进行前后页的查找
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

			//_idSpanMap.set(nSpan->_pageId, nSpan);
			//_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			//建立页号与span的映射，方便central cache回收小块内存时
			// 查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				_idSpanMap[kSpan->_pageId + i] = kSpan;
				//_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}
			return kSpan;
		}
	}

	//到这里说明后面没有大页的span了，这时就像堆申请一个128页的span
	Span* bigSpan = new Span;
	//Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);

	//进行递归调用
	return NewSpan(k);
}


//获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;    //页号

	std::unique_lock<std::mutex> lock(_pageMtx);    //构造时加锁，析构时自动解锁
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}

	//Span* ret = (Span*)_idSpanMap.get(id);
	//assert(ret != nullptr);
	//return ret;
}

//释放空闲的span回到PageCache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n > NPAGES - 1)    //大于128页直接释放给堆
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		//_spanPool.Delete(span);

		return;
	}
	//对span的前后页，尝试进行合并，缓解内存碎片

	//向前合并
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		auto ret = _idSpanMap.find(prevId);
		//前面的页号没有，则停止向前合并
		if (ret == _idSpanMap.end())
		{
			break;
		}
		//Span* ret = (Span*)_idSpanMap.get(prevId);
		//if (ret == nullptr)
		//{
		//	break;
		//}

		//前面的页号对应的span正在被使用，停止合并
		Span* prevSpan = ret->second;
		//Span* prevSpan = ret;
		if (prevSpan->_isUse == true)
		{
			break;
		}

		//合并出超过128页的span无法进行管理，停止向前合并
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		//进行向前合并
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		//将prevSpan从对应的双链表中移除
		_spanLists[prevSpan->_n].Erase(prevSpan);

		//释放prevspan
		delete prevSpan;
	}

	//向后合并
	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;

		auto ret = _idSpanMap.find(nextId);
		//后面的页号没有（还未向系统申请），停止向后合并
		if (ret == _idSpanMap.end())
		{
			break;
		}
		//Span* ret = (Span*)_idSpanMap.get(nextId);
		//if (ret == nullptr)
		//{
		//	break;
		//}

		//后面的页号对应的span正在被使用，停止向后合并
		Span* nextSpan = ret->second;
		//Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		//合并出超过128页的span无法进行管理，停止合并
		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		//进行向后合并
		span->_n += nextSpan->_n;

		//移除
		_spanLists[nextSpan->_n].Erase(nextSpan);

		//delete
		delete nextSpan;
	}

	//合并后的加入双链表中
	_spanLists[span->_n].PushFront(span);

	//建立对应的映射
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;

	//_idSpanMap.set(span->_pageId, span);
	//_idSpanMap.set(span->_pageId + span->_n - 1, span);

	//设置当前span的状态为未使用
	span->_isUse = false;
}