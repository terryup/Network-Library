#ifndef CENTRALCACHE_HPP
#define CENTRALCACHE_HPP

#include "common.hpp"

class CentralCache
{
private:
    static CentralCache _inst;

    //  构造函数默认化，也就是无参无内容
	CentralCache() {}
    CentralCache(const CentralCache&)  = delete;

    SpanList _spanlist[NFREELIST];

public:
    //  获取单例
    static CentralCache* GetInstance()
    {
        return &_inst;
    }

    //  从中心缓存获取一定数量的内存给thread cache
    size_t fetch_range_obj(void*& start, void*& end, size_t batchNum, size_t size);

    //  从span链表数组中拿出和bytes相等的span链表
    Span* get_one_Span(SpanList& list, size_t byte_size);

    //  将ThreadCache中的内存块归还给CentralCache
    void release_list_to_Spans(void* start, size_t byte_size);

};



#endif