#ifndef THREADCACHE_HPP
#define THREADCACHE_HPP

#include "common.hpp"

class ThreadCache
{
private:
    //  创建了一个自由链表数组，长度为NLISTS是240
    //  长度计算时根据对齐规则得来的
    FreeList _freelist[NFREELIST];

public:
    //  分配内存
    void* Allocate(size_t size);

    //  释放内存，这里不是真的释放，只是放回来
    void deallocate(void* ptr, size_t size);

    //  从中心缓存中拿取内存
    void* fetch_from_CentralCache(size_t index, size_t size);

    //  当链表中的对象太多的时候开始回收到中心缓存
    void list_to_long(FreeList& freelist, size_t byte);

};


//  静态的tls变量，每一个ThreadCache对象都有着自己的一个tls_threadcache
//  __thread相当于每一个线程都有一个属于自己的全局变量
static __thread ThreadCache* tls_threadcache = nullptr;

#endif