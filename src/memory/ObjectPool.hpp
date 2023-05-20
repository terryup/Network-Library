#ifndef OBJECTPOOL_HPP
#define OBJECTPOOL_HPP

#include "common.hpp"

template <class T>
class objectPool
{
private:
    char* _memory = nullptr;    //  管理大块内存
	size_t remainBytes = 0; //  记录_memory管理的大块内存的数量
	void* _freeList = nullptr;  //  管理释放回来的内存

public:
    T* New()
    {
        T* obj = nullptr;
        if(_freeList)
        {
            obj = (T*)_freeList;
            _freeList = NEXT_OBJ(_freeList);
        }
        else
        {
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            //  如果所剩内存大小不足一个objSize那么就要申请一块大的
            if (remainBytes < objSize)
            {
                //  假设每次申请 128KB 内存
                remainBytes = 128 * 1024;
                //  按页为单位获取内存，假设一页为 8KB
                _memory =(char*) SystemAlloc_sbrk(remainBytes >> 13);
            }
            obj = (T*)_memory;
			remainBytes -= objSize;
			_memory += objSize;
        }
        new(obj)T;  //  定位new表达式，显示调用T类的构造函数进行初始化
        return obj;

    }

    //  将释放的obj交给_freeList
    void Delete(T *obj)
    {
        obj->~T();
        NEXT_OBJ((void*)obj) = _freeList;
        _freeList = obj;
    }
};


#endif