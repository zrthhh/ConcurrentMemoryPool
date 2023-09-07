#include "ThreadCache.h"
#include "CentralCache.h"



//申请内存对象
void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);    //thread cache只处理小于MAX_BYTES的申请
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		return FetchFromCentralCache(index, alignSize);
	}
}

//释放内存对象
void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(ptr);
	assert(size <= MAX_BYTES);

	//找出对应的自由链表中的哈希桶将释放的对象插入
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	//当回收后，链表的长度大于一次批量申请的对象个数时
	//就返还一段对象给central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//慢开始反馈调节算法
	//1.最开始不会一次性向central cache申请太多对象内存
	//2.如果不断有size大小的内存需求，batchNum会不断增长，直到上限
	size_t batNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batNum == _freeLists[index].MaxSize()) {
		_freeLists[index].MaxSize() += 1;
	}
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batNum, size);
	assert(actualNum >= 1);    //至少申请一个

	if (actualNum == 1)    //申请到了一个
	{
		assert(start == end);
		return start;
	}
	else    //申请到多个，将剩下的挂载到对应的哈希桶的自由链表中
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

//释放对象后导致链表过长，回收内存到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	//从链表中取出对象
	list.PopRange(start, end, list.MaxSize());

	//将对象归还到central cache中对应的span中
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}