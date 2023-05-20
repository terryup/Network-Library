#include "CentralCache.hpp"
#include"PageCache.hpp"
CentralCache CentralCache::_inst;

//  从span链表数组中拿出和bytes相等的span链表
Span* CentralCache::get_one_Span(SpanList& List, size_t size)
{
	//	在哈希桶对应位置Span链表中找是否有Span，没有就向PageCache申请空间
	//	遍历桶的Span链表
	Span* it = List.begin();
	while (it != List.end()) 
	{
		if (it->_objlist != nullptr) 
		{
			return it;//这个Span有空间
		}
		else 
		{
			//	Span没有空间，继续找下一个链表Span
			it = it->_next;
		}
	}
	//	先把CentralCache的桶锁解开，如果其他线程释放内存不会阻塞
	List._mtx.unlock();
	//	没有空闲的Span只能找PageCache,需要加锁，PageCache只能由一个线程访问
	//	size是单个对象的大小
	PageCache::GetInstance()->_mtx.lock();
	//  需要计算要获取几页npage，然后直接获取
	Span* span=PageCache::GetInstance()->new_Span(classSize::num_move_page(size));
	span->_isUse = true;
	span->_objsize = size;	//	保存size，为了让ConcurrentFree 释放空间时不需要传大小
	PageCache::GetInstance()->_mtx.unlock();

	//	获得了一块大Span，这块Span这时被线程单独看到，不需要加锁（没有挂到桶上）
	//	Span起始地址
	char* start = (char*)((span->_pageid) << PAGE_SHIFT);
	size_t ByteSize = (span->_npage) << PAGE_SHIFT;
	char* end = start + ByteSize;
	//把Span内部大块内存切成自由链表链接起来
	span->_objlist = start;
	start += size;//自由链表的头节点
	void* tail = span->_objlist;
	while (start < end) 
	{
		NEXT_OBJ(tail) = start;
		tail = NEXT_OBJ(tail);
		start += size;
	}
	NEXT_OBJ(tail) = nullptr;
	List._mtx.lock();
	List.push_front(span);
	return span;
}

//  从中心缓存获取一定数量的内存给thread cache
size_t CentralCache::fetch_range_obj(void*& start, void*& end, size_t Num, size_t size)
{
	size_t index = classSize::index(size);
	//要求num个内存节点，需要计Span中FreeList空节点个数
	_spanlist[index]._mtx.lock();//加锁

	//	调用上面这个函数，从spanlist获取一个span，也就是获取一个内存块
	Span* span = get_one_Span(_spanlist[index], size);
	assert(span);
	assert(span->_objlist);

	//	start就是prev的下一个内存块的地址，就是尾后
	start = span->_objlist; end = start;

	//变量Num次，找Num个节点
	size_t actualNum = 1;//实际获得几个内存节点
	for (int i = 0; i < Num - 1; i++) 
	{
		if (NEXT_OBJ(end) == nullptr)
		{
			//	如果走到span中FreeList的空，说明span中内存不够Num个，这个时候有多少返回多少
			break;
		}
		actualNum += 1;
		end = NEXT_OBJ(end);
	}
	span->_objlist = NEXT_OBJ(end);
	//为分出去的内存节点链表添加nullptr
	NEXT_OBJ(end) = nullptr;

	span->_usecount += actualNum;

	_spanlist[index]._mtx.unlock();
	return actualNum;
}

//	将ThreadCache中的内存块归还给CentralCache
void CentralCache::release_list_to_Spans(void* start, size_t size)
{
	//	找到对应的spanlist
	size_t index = classSize::index(size);

	// CentralCache:对当前桶进行加锁(桶锁)
	// PageCache:必须对整个SpanList全局加锁
	// 因为可能存在多个线程同时去系统申请内存的情况
	_spanlist[index]._mtx.lock();

	//	遍历start那条空闲链表，从新连到span的_objlist中
	while (start != nullptr) 
	{
		void* next = NEXT_OBJ(start);

		//  获取内存对象到span的映射
		Span* span = PageCache::GetInstance()->map_object_to_Span(start);

		//将这个节点头插到这个Span上
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		span->_usecount--;//每回收一块内存，记录分出去的内存use_count就-1

		if (span->_usecount == 0) 
		{
			//Span所有小块内存都回来了，这个Span就可以给Page Cache回收。PageCache做前后页的合并
			_spanlist[index].erase(span);

			//通过页号就可以再次找到这块内存
			span->_objlist = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			//页号与页数不能动，PageCache通过这两个数据找到这大块内存,此时可以解开桶锁，让其他线程访问这个桶
			_spanlist[index]._mtx.unlock();
			
			// 将一个span从CentralCache归还到PageCache的时候只需要页号和页数
			// 不需要其他的东西所以对于其他的数据进行赋空
            //  PageCache是全局共享资源
            //  这个地方必须上把锁，不然会造成多个线程同时归还的问题
			PageCache::GetInstance()->_mtx.lock();
			PageCache::GetInstance()->relase_to_PageCache(span);
			PageCache::GetInstance()->_mtx.unlock();
			_spanlist[index]._mtx.lock();
		}
		start = next;
	}

	_spanlist[index]._mtx.unlock();
}