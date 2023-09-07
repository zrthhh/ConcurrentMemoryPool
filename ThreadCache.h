#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//�����ڴ����
	void* Allocate(size_t size);

	//�ͷ��ڴ����
	void Deallocate(void* ptr, size_t size);

	//��central cache�л�ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);

	//�ͷŶ�����������������������central cache
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[NFREELISTS];    //������ϣͰ
};

//TLS����
/*�ֲ߳̾��洢TLS
* ʹ�øô洢�����ı����������ڵ��߳���ȫ�ֿɷ��ʵ�
* ���ǲ��ܱ������̷߳��ʵ��������ͱ��������ݵ��̶߳�����
*/
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;

