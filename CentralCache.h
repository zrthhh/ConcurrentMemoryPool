#pragma once

#include "Common.h"

//����ģʽ
class CentralCache
{
public:
	//�ṩһ��ȫ�ַ��ʵ�
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//��central cache��ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	//��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& spanList, size_t size);

	//��һ�������Ķ���黹����Ӧ��span
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _spanLists[NFREELISTS];    //span�б�

private:
	//���캯��˽��
	CentralCache(){}

	CentralCache(const CentralCache&) = delete;    //��ֹ����

	static CentralCache _sInst;
};

