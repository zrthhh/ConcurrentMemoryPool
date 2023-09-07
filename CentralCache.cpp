#include "CentralCache.h"
#include "PageCache.h"


CentralCache CentralCache::_sInst;
ObjectPool<Span> SpanList::_spanPool;

//接收到thread cache的申请后，central cache分配一定
//数量的对象大小给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)
{
	size_t index = SizeClass::Index(size);

	//给对应的哈希桶加锁
	_spanLists[index]._mtx.lock();

	//在对应的哈希桶中获取一个非空的span
	Span* span = GetOneSpan(_spanLists[index], size);
	//判断span不为空，挂载的自由链表是否非空
	assert(span);
	assert(span->_freeList);

	//从span中获取n个对象
	//如果不够n个，有多少拿多少
	start = span->_freeList;
	end = span->_freeList;

	size_t actualNum = 1;
	while (NextObj(end) && n - 1)
	{
		end = NextObj(end);
		actualNum++;
		n--;
	}
	span->_freeList = NextObj(end);    //取完后剩下的对象继续放到自由链表中
	NextObj(end) = nullptr;    //取出的一段链表的表位置空
	span->_useCount += actualNum;    //更新被分配给thread cache的计数

	_spanLists[index]._mtx.unlock();    //给桶解锁


	return actualNum;
}

//获取一个非空的span从page cache中
Span* CentralCache::GetOneSpan(SpanList& spanList, size_t size)
{
	// 1.先在spanlist中寻找到非空的span
	Span* it = spanList.Begin();
	while (it != spanList.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}

	// 2.spanList中没有非空的span，只能向page cache中申请
	//先吧central cache的桶锁解掉，这样如果其他对象释放内存对象回来，不会阻塞
	spanList._mtx.unlock();
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;    //指定span的分割大小
	PageCache::GetInstance()->_pageMtx.unlock();
	//获取到span后不需要立刻重新加上central cache的桶锁

	//计算span的大块内存的起始地址和大块内存的大小(字节数)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;

	//把大块内存切成size大小的对象并链接起来
	char* end = start + bytes;

	//先切一块下来去做尾，方便尾插
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	
	//尾插
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	//将切好的span头插到spanList中
	spanList._mtx.lock();    //span切分完毕后，需要挂到桶的时候需要进行加锁
	spanList.PushFront(span);

	return span;
}

//将一定数量的对象归还给对应的span
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();  //加锁
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//将对象头插入到span的自由链表
		NextObj(start) = span->_freeList;
		span->_freeList = start;

		span->_useCount--;    //更新被分配给thread cache的计数
		if (span->_useCount == 0)    //为0时，说明没有分配给thread cache，可以考虑回收到page cache
		{
			//此时这个span就可以再回收给page cache，page cache 可以再尝试去做页的合并
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;  //自由链表置空
			span->_next = nullptr;
			span->_prev = nullptr;

			//释放span给page cache时，使用page cache的锁就可以了，这时把桶锁解掉
			_spanLists[index]._mtx.unlock();  //解锁桶
			PageCache::GetInstance()->_pageMtx.lock(); //加大锁
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock(); //解大锁
			_spanLists[index]._mtx.lock(); //加桶锁
		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();  //解锁
}