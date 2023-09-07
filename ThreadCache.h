#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//申请内存对象
	void* Allocate(size_t size);

	//释放内存对象
	void Deallocate(void* ptr, size_t size);

	//从central cache中获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	//释放对象导致链表过长，择机回收至central cache
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[NFREELISTS];    //创建哈希桶
};

//TLS策略
/*线程局部存储TLS
* 使用该存储方法的变量在它所在的线程是全局可访问的
* 但是不能被其他线程访问到，这样就保持了数据的线程独立性
*/
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;

