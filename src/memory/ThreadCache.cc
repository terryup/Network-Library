#include "ThreadCache.hpp"
#include "CentralCache.hpp"
#include "common.hpp"

//  分配内存
//  从自由链表数组的自由链表上拿取内存对象
void* ThreadCache::Allocate(size_t size) 
{
	assert(size <= MAXBYTES);
	size_t AligSize = classSize::round_up(size);

	//	计算桶位置
	size_t index = classSize::index(AligSize);
	if (!_freelist[index].empty()) 
	{
		return _freelist[index].pop();
	}
	else 
	{
		//	thread cache没有内存向central cache要空间
		return fetch_from_CentralCache(index, AligSize);
	}
}

void ThreadCache::deallocate(void *ptr, size_t size) 
{
	assert(ptr);
    assert(size <= MAXBYTES);

	size_t index = classSize::index(size);//找到第几个桶

	//	将这个空间头插到自由链表上
	_freelist[index].push(ptr);

	//	如果FreeList中空间节点超过要申请的长度，回收内存
	if (_freelist[index].maxSize() < _freelist[index].size()) 
	{
		//	size是每个节点的大小
		list_to_long(_freelist[index], size);
	}
}

void* ThreadCache::fetch_from_CentralCache(size_t index, size_t size) 
{
	//	慢开始调节算法
	size_t Num = std::min(classSize::num_move_size(size), _freelist[index].maxSize());//计算中心层给线程缓存多少个空间节点;
	if (Num == _freelist[index].maxSize()) 
	{
		//	慢增长,每次申请下次会多给ThreadCache空间
		_freelist[index].maxSize() += 1;
	}

	//	依次递增SizeClass::ForMemory(size)是申请上限，一次申请数量不会比它还大
	void* begin = nullptr; void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->fetch_range_obj(begin, end, Num, size);

	//	至少会申请到一个空间节点
	assert(actualNum >= 1);
	if (actualNum == 1) 
	{
		//	实际就获得了一个节点,直接将这个节点返回
		return begin;
	}
	else 
	{
		//	获得了多个节点，要把这些节点都插入到ThreadCache的哈希桶上
		//	//将链表的下一个节点插入桶中，头节点返回给ThreadCache
		_freelist[index].PushRange(NEXT_OBJ(begin), end, actualNum - 1);
		return begin;
	}
}

//  当链表中的对象太多的时候开始回收到中心缓存
void ThreadCache::list_to_long(FreeList& List, size_t size) {
	void* start = nullptr; 
	void* end = nullptr;
	//	一次获取List.GetMaxSize()个节点的链表
	List.PopRange(start, end, List.maxSize());

	CentralCache::GetInstance()->release_list_to_Spans(start, size);
}