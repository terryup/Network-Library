#pragma once
#include "common.hpp"
#include "ThreadCache.hpp"
#include "PageCache.hpp"
#include "ObjectPool.hpp"

//  用户申请内存
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES) 
	{
		//  申请的内存大于64K,就直接到PageCache中申请内存
		size_t AlignSize = classSize::round_up(size);//计算对其大小
		//	直接向PageCache索要K页的内存
		size_t K = AlignSize >> PAGE_SHIFT;
		PageCache::GetInstance()->_mtx.lock();
		Span*span=PageCache::GetInstance()->new_Span(K);

		//	没有经过CentralCache直接获取的Span填写ObjectSize,为了ConcurrentFree不传size参数
		span->_objsize = size;

		PageCache::GetInstance()->_mtx.unlock();

		//	获取这块内存的地址
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else 
	{
		//获取线程自己的ThreadCache
		if (tls_threadcache == nullptr) 
		{
			static objectPool <ThreadCache>TcPool;
			tls_threadcache = TcPool.New();
		}
		//cout << std::this_thread::get_id() << " " << tls_threadcache << endl;
		return tls_threadcache->Allocate(size);
	}
}

//  用户释放内存
static void ConcurrentFree(void* ptr)
{
	//  获取页号到span的映射
	Span* span = PageCache::GetInstance()->map_object_to_Span(ptr);//计算要释放的大空间属于那个Span
	size_t size = span->_objsize;

	//  是大于64K,直接归还给PageCache
	if(size > MAXBYTES)
	{
		PageCache::GetInstance()->_mtx.lock();
        PageCache::GetInstance()->relase_to_PageCache(span);
        PageCache::GetInstance()->_mtx.unlock();
	}
	else 
	{
		//释放时每个线程一定有tls_threadcache
		assert(tls_threadcache != nullptr);
		tls_threadcache->deallocate(ptr, size);
	}
}