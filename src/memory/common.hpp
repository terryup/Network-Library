#pragma once
#include <iostream>
#include <vector>
#include <ctime>
#include <assert.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <unistd.h>
#include <type_traits>
#include <limits>



static const size_t MAXBYTES = 256 * 1024;	//	所申请内存的最大容量

static const size_t NFREELIST = 208;	//	线程缓存中的freeLists数组大小

static const size_t NPAGES = 129;	//	页缓存中_spanLists数组大小

static const size_t PAGE_SHIFT = 13;	//	一页是4096字节,2的12次方=4096

typedef size_t PageID;

typedef uint8_t CompactSizeClass;

inline static void*SystemAlloc_sbrk(size_t kpage) 
{

	void *ptr = sbrk(kpage << 13);
	//std::cout << "sbrk分配成功" << std::endl;
	if (ptr == nullptr) 
	{
		throw std::bad_alloc();
	}
	return ptr;
}

inline static void SystemFree_brk(void* ptr)
{
	brk(ptr);
}

static void*& NEXT_OBJ(void*obj) 
{
	//  这个位置就是先通过(void **)强制转换为void**类型->这里只是告诉编译器，这个obj是一个指向void*类型的指针
    //  然后通过*解引用，得到原来obj他所指向的值，而这个时候因为我们已经提前强制转换了，那么这个值就是一个void*
    //  而不是原来的void*所指向的地址
    /* 
        int i = 40;
        void * ptr = &i;---->这个时候ptr这个指针是int i他在内存中的地址
        ptr = (void**)ptr;--->这个时候就是告诉编译器，ptr变为了一个void**类型的指针，也就是它指向的不是int了是void*
        void *ptr1 = *(ptr);---->这个时候在解引用那就是吧原来的int型的数据强转为了void *型的一个指针
    */
	return *((void**)obj);
}

//维护一个头指针
class FreeList 
{
private:
	void*_head = nullptr;
	size_t _maxSize = 1;//用于控制慢启动的成员
	size_t _size = 0;////记录当前所挂的对象数量
public:
	//头插
	void push(void*obj) 
	{
		NEXT_OBJ(obj) = _head;
		_head = obj;
		++_size;
	}
	//头插一批
	void PushRange(void* start, void* end,size_t n)
	{
		NEXT_OBJ(end) = _head;
		_head = start;
		_size += n;
	}

	//头删一批
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n >= _size);
		start = _head;
		end = start;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NEXT_OBJ(end);
		}

		_head = NEXT_OBJ(end);
		NEXT_OBJ(end) = nullptr;
		_size -= n;
	}

	//头删并返回
	void*pop() 
	{
		void*obj = _head;
		_head = NEXT_OBJ(_head);
		--_size;
		return obj;
	}

	bool empty() 
	{
		return _head == nullptr;
	}

	size_t& maxSize()
	{
		return _maxSize;
	}

	size_t size()
	{
		return _size;
	}

};

// 计算对象大小的对齐映射规则
class classSize
{
public:
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)

	static inline size_t _round_up(size_t bytes, size_t alignNum) 
	{
		return (bytes + alignNum - 1)&~(alignNum - 1);
	}

	//计算出要获取的单个obj的大小
	//比如size为7那么单个obj大小为8
	//比如size为123那么单个obj大小为128
	static inline size_t round_up(size_t size) 
	{
		if (size <= 128) 
		{
			return _round_up(size, 8);
		}
		else if (size <= 1024) 
		{
			return _round_up(size, 16);
		}
		else if (size <= 8*1024) 
		{
			return _round_up(size, 128);
		}
		else if (size <= 64*1024) 
		{
			return _round_up(size, 1024);
		}
		else if (size <= 256*1024) 
		{
			return _round_up(size, 8*1024);
		}
		else 
		{
			assert(false);
		}
		return -1;
	}

	static inline size_t _index(size_t bytes, size_t align_shift) 
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//	计算映射的哪一个自由链表桶，比如bytes为7，映射在_freeLists下标为0处的自由链表桶
	static inline size_t index(size_t bytes) 
	{
		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) 
		{
			return _index(bytes, 3);
		}
		else if (bytes <= 1024) 
		{
			return _index(bytes - 128, 4)+group_array[0];
		}
		else if (bytes <= 8*1024) 
		{
			return _index(bytes - 1024, 7)+ group_array[0]+ group_array[1];
		}
		else if (bytes <= 64*1024) 
		{
			return _index(bytes - 8*1024, 10) + group_array[0] + group_array[1]+group_array[2];
		}
		else if (bytes <= 256*1024) 
		{
			return _index(bytes - 64*1024, 13) + group_array[0] + group_array[1] + group_array[2]+group_array[3];
		}
		else 
		{
			assert(false);
		}
		return -1;
	}

	// 一次thread cache从中心缓存获取多少个
	static size_t num_move_size(size_t size)
	{
		assert(size > 0);

		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAXBYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 64KB
	//小对象就分配的页数少一点
	//大对象分配的页数就多一点
	static size_t num_move_page(size_t size) 
	{
		size_t num = num_move_size(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		//如果单个对象8字节，那么就分配一页（8kb)；如果单个对象是64kb（1kb=2^10b）,那么就分配32页
		if (npage == 0)npage = 1;

		return npage;
	}
};

//page cache与central cache都要用span
// 管理以页为单位的大块内存
struct Span 
{
	//这两个参数主要是给page cache使用的
	PageID _pageid;   // 页号
	size_t _npage;        // 页的数量

	Span* _next = nullptr;//可能会申请多个大块内存，即有多个span,因此需要链起来，好还给系统
	Span* _prev = nullptr;
	//当有多个span的时候，要取出一个span，那么使用双向链表SpanList就可以直接取了

	void* _objlist = nullptr;  //当该成员变量为空时就表明该span被用完了
	//用_freeList指向切小的大块内存，这样回收回来的内存也方便链接（span由一个个页构成，因为central cache
	//先切成一个个对象再给thread cache，span用_freeList指向切小的大块内存）

	size_t _objsize = 0;  // 切好的小对象的大小
	size_t _usecount = 0;     // 使用计数
	bool _isUse = false;          // 是否在被使用
};

//维护了一个头结点与一个桶锁
class SpanList 
{
//管理大块内存的带头双向循环链表
public:
	SpanList() 
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* begin()
	{
		return _head->_next;
	}

	Span* end()
	{
		return _head;
	}

	void push_front(Span* span)
	{
		insert(begin(), span);
	}

	Span* pop_front()
	{
		Span* front = _head->_next;
		erase(front);
		return front;
	}

	void insert(Span* cur, Span* newspan) 
	{
		assert(cur);
		assert(newspan);
		Span* prev = cur->_prev;
		// prev newspan cur
		prev->_next = newspan;
		newspan->_prev = prev;

		newspan->_next = cur;
		cur->_prev = newspan;
	}

	void erase(Span* cur) 
	{
		//不需要delete掉Span，因为还需要将Span返给page cache
		assert(cur);
		if (cur == _head) {
			std::cout << "error" << std::endl;
		}

		assert(cur != _head);

		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	bool empty() 
	{
		return _head->_next == _head;
	}

private:
	Span* _head;
public:
	std::mutex _mtx; // 桶锁
};

