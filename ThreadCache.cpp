#include "ThreadCache.h"
#include "CentralCache.h"



//�����ڴ����
void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);    //thread cacheֻ����С��MAX_BYTES������
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

//�ͷ��ڴ����
void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(ptr);
	assert(size <= MAX_BYTES);

	//�ҳ���Ӧ�����������еĹ�ϣͰ���ͷŵĶ������
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	//�����պ�����ĳ��ȴ���һ����������Ķ������ʱ
	//�ͷ���һ�ζ����central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//����ʼ���������㷨
	//1.�ʼ����һ������central cache����̫������ڴ�
	//2.���������size��С���ڴ�����batchNum�᲻��������ֱ������
	size_t batNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batNum == _freeLists[index].MaxSize()) {
		_freeLists[index].MaxSize() += 1;
	}
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batNum, size);
	assert(actualNum >= 1);    //��������һ��

	if (actualNum == 1)    //���뵽��һ��
	{
		assert(start == end);
		return start;
	}
	else    //���뵽�������ʣ�µĹ��ص���Ӧ�Ĺ�ϣͰ������������
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

//�ͷŶ��������������������ڴ浽���Ļ���
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	//��������ȡ������
	list.PopRange(start, end, list.MaxSize());

	//������黹��central cache�ж�Ӧ��span��
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}