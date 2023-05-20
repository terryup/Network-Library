#ifndef PAGECACHE_HPP
#define PAGECACHE_HPP

#include "common.hpp"
#include "ObjectPool.hpp"
#include "PageMap.hpp"

class PageCache
{
private:
	//std::unordered_map<PageID, Span*> _id_span_map;

    PageCache() = default;
	PageCache(const PageCache&) = delete;
	//PageCache& operator=(const PageCache&) = delete;
	static PageCache _inst;

    //  NPAGES是129，但是使用128个数据元素，也就是下标从1开始到128分别为1页到128页
    SpanList _pagelist[NPAGES];

    //  使用定长内存池代替new
    objectPool<Span> _spanPool;

    //  问题二： 这个位置要给48，因为是虚拟地址
    TCMalloc_PageMap3<48 - PAGE_SHIFT> _id_span_map;

public:
    std::mutex _mtx;

    static PageCache* GetInstance()
    {
        return &_inst;
    }

    //  从系统申请span或者大于要申请的npage的Pagespan中申请
    Span* new_Span(size_t npage);

    //Span* _new_Span(size_t npage);

    //  获取从对象到span的映射
    Span* map_object_to_Span(void* obj);

    //  从CentralCache归还span到Page
    void relase_to_PageCache(Span* span);

};



#endif