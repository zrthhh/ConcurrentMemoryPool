#include "ConcurrentAlloc.h"
#include "ObjectPool.h"

using std::endl;
using std::cout;

//ntimes�����ִ�������ͷ��ڴ�Ĵ���
//nworks���߳���
//rounds���ִΣ��ܶ����֣�
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
					//v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�malloc" << ntimes <<
		"��: ���ѣ�" << malloc_costtime << "ms" << endl;
	cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�free" << ntimes <<
		"��: ���ѣ�" << free_costtime << "ms" << endl;
	cout << nworks << "���̲߳���malloc&free" << nworks * rounds * ntimes << "�Σ��ܼƻ��ѣ�" <<
		malloc_costtime + free_costtime << "ms" << endl;
}

void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
					//v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�concurrent alloc" <<
		ntimes << "��: ���ѣ�" << malloc_costtime << "ms" << endl;
	cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�concurrent dealloc" <<
		ntimes << "��: ���ѣ�" << free_costtime << "ms" << endl;
	cout << nworks << "���̲߳���concurrent alloc&dealloc" << nworks * rounds * ntimes << "�Σ��ܼƻ��ѣ�" <<
		malloc_costtime + free_costtime << "ms" << endl;
}

int main()
{
	size_t n = 10000;
	std::cout << "==========================================================" <<
		std::endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	std::cout << std::endl << std::endl;
	BenchmarkMalloc(n, 4, 10);
	std::cout << "==========================================================" <<
		std::endl;
	return 0;
}