#include "PageCache.hpp"

PageCache PageCache::_inst;

//	从系统申请span或者大于要申请的npage的Pagespan中申请
Span* PageCache::new_Span(size_t NumPage){
	assert(NumPage > 0);

	if (NumPage >= NPAGES) 
	{
		//如果要申请的内存大于桶的个数，直接向堆申请空间
		void* ptr = SystemAlloc_sbrk(NumPage);

		//	构造span去管理从系统申请的内存
		Span* span = _spanPool.New();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = NumPage;
		span->_objsize = NumPage << PAGE_SHIFT;

		//保存起始页号，方便释放内存
		//_id_span_map[span->_pageid] = span;

		_id_span_map.set(span->_pageid, span);
		return span;
	}
	else 
	{
		//看当前位置桶中是否有Span
		if (!_pagelist[NumPage].empty()) 
		{
			Span*NumPageSpan =_pagelist[NumPage].pop_front();
			for (PageID i = 0; i < NumPageSpan->_npage; i++) 
			{
				//切了Num页
				//_id_span_map[NumPageSpan->_pageid + i] = NumPageSpan;

				_id_span_map.set(NumPageSpan->_pageid, NumPageSpan);
			}
			return NumPageSpan;
		}

		//对应位置的桶是空，检查后面桶里有没有Span,将大Span切分成小Span
		for (size_t i = NumPage + 1; i < NPAGES; i++) 
		{
			if (!_pagelist[i].empty()) 
			{
				//有一个桶存在，切分大Span成NumPage页Span和N-NumPage页的Span
				Span* NumPageSpan = _spanPool.New();
				Span* NSpan = _pagelist[i].pop_front();
				
				//头切
				NumPageSpan->_pageid = NSpan->_pageid;
				NumPageSpan->_npage = NumPage;
				NSpan->_pageid += NumPage;
				NSpan->_npage -= NumPage;

				//将切下的Span挂到其他桶上
				_pagelist[NSpan->_npage].push_front(NSpan);
				//_SpanList[NSpan->_npage].Insert(_SpanList[NSpan->_npage].begin(), NSpan);

				
				//保存NSpan前后页的映射关系，方便回收
				//_id_span_map[NSpan->_pageid] = NSpan;
				//_id_span_map[NSpan->_pageid + NSpan->_npage - 1] = NSpan;
				//std::cout << NSpan->_pageid << std::endl; 
				//_id_span_map.Ensure(NSpan->_pageid, NSpan->_objsize);
				//	问题一： 这里报断错误-已解决
				_id_span_map.set(NSpan->_pageid, NSpan);
				_id_span_map.set(NSpan->_pageid + NSpan->_npage - 1, NSpan);//中间页没必要加入映射中，前后两页为了方便回收

				//填入PAGE_ID与Span*映射中
				//_id_span_map.Ensure(NumPageSpan->_pageid, NumPageSpan->_objsize);
				for (size_t i = 0; i < NumPageSpan->_npage; i++) 
				{
					//切了Num页
					//_id_span_map[NumPageSpan->_pageid + i] = NumPageSpan;
					//std::cout << NumPageSpan->_pageid << std::endl;
					_id_span_map.set(NumPageSpan->_pageid + i, NumPageSpan);
				}
				//	std::cout << "出来了" << std::endl;

				return NumPageSpan;
			}
		}
		//所有桶都没有Span,直接向堆中申请一大块空间，将这块空间挂到最后一个桶上
		Span* BigSpan = _spanPool.New();
		void* ptr = SystemAlloc_sbrk(NPAGES - 1);
		//_id_span_map.PreallocateMoreMemory();
		BigSpan->_pageid = (PageID)ptr >> PAGE_SHIFT;
		BigSpan->_npage = NPAGES - 1;
		BigSpan->_objsize = (NPAGES - 1) << PAGE_SHIFT;
		//	问题三： 这里是初始化的问题，范围应该在sbrk这里初始化好
		_id_span_map.Ensure(BigSpan->_pageid, BigSpan->_objsize);
		_pagelist[BigSpan->_npage].push_front(BigSpan);
		//_SpanList[BigSpan->_Num].Insert(_SpanList[BigSpan->_Num].begin(), BigSpan);
		//在调用自己向Central Cache发送空间
		//std::cout << BigSpan->_pageid << std::endl;
		return new_Span(NumPage);
	}
}

//  获取从对象到span的映射
Span* PageCache::map_object_to_Span(void* obj)
{
	std::unique_lock<std::mutex>lock(_mtx);//映射被多个线程访问需要加锁防止线程安全，出了函数锁自动释放
	// 使用基数树后不需要加锁
	//计算obj的页号
	PageID pageId = (PageID)obj >> PAGE_SHIFT;
	//获取这个内存是那个Span
	//auto it = _id_span_map.find(pageId);
	Span* ret =(Span*)_id_span_map.get(pageId);
	assert(ret != nullptr);

    //assert(it != _id_span_map.end());
	return ret;
}


//  将CentralCache的span归还给PageCache进行合并(减少内存碎片:外碎片)
void PageCache::relase_to_PageCache(Span* span)
{
	if (span->_npage >= NPAGES) 
	{
		//直接向堆申请的空间大于128页的还给系统，其余的挂在桶上
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		SystemFree_brk(ptr);
		_spanPool.Delete(span);
	}
	//	合并过程:先向前合并，然后跳过该span再次向后合并
	//	找到这个span前面一个span
	else 
	{
		//对Span前后页进行合并，缓解内存碎片外碎片问题
		while (true) 
		{ 
			//向前合并
			PageID prevId = span->_pageid - 1;
			Span* ret = (Span*)_id_span_map.get(prevId);
			//Span* ret = (Span*)IdSpanMap.get(prevId);
			if (ret == nullptr) {
				break;
			}
			else {
				if (ret->_isUse == true) 
				{
					//前面相邻页的Span正在使用的内存不合并
					break;
				}
				//和前Span合并大小超过128KB则停止合并,无法管理
				else if (ret->_npage + span->_npage >= NPAGES) 
				{
					break;
				}
				else 
				{
					//合并
					span->_pageid = ret->_pageid;
					span->_npage += ret->_npage;
					_pagelist[ret->_npage].erase(ret);
					_spanPool.Delete(ret);
				}
			}
		}
		//向后合并
		//	找到这个span后面的span(要跳过该span)
		while (true) 
		{
			PageID nextId = span->_pageid + span->_npage;
			Span* ret = (Span*)_id_span_map.get(nextId);
			//Span* ret = (Span*)IdSpanMap.get(nextId);
			if (ret == nullptr) 
			{
				break;
			}
			else 
			{
				Span* nextSpan = ret;
				//	判断后边span的计数是不是0
				if (nextSpan->_isUse == true) 
				{
					break;
				}
				//	判断前面的span加上后面的span有没有超出NPAGES
				else if (nextSpan->_npage + span->_npage >= NPAGES) 
				{
					break;
				}
				else 
				{
					span->_npage += nextSpan->_npage;
					_pagelist[nextSpan->_npage].erase(nextSpan);
					_spanPool.Delete(nextSpan);
				}
			}
		}
		//将合并的Span挂起并添加映射
		_pagelist[span->_npage].push_front(span);
		span->_isUse = false;
		//_id_span_map[span->_pageid] = span;
		//_id_span_map[span->_pageid + span->_npage - 1] = span;
		_id_span_map.set(span->_pageid, span);
		_id_span_map.set(span->_pageid + span->_npage - 1, span);
	}
}
