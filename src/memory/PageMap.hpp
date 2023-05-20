#ifndef PAGEMAP_HPP
#define PAGEMAP_HPP

#include "common.hpp"
#include <cstring>

static constexpr size_t HUGEPAGESHIIFT = 21;

static constexpr size_t HUGEPAGESIZE = static_cast<size_t>(1) << HUGEPAGESHIIFT;

//	这里实现了一个三层的基数树，用来优化PageCache用unordered_map映射导致资源长时间被占用问题
template <int BITS>
class TCMalloc_PageMap3
{
private:
	//	表示在每个内部层级上应该消耗的位数
	//	加2是为了确保在整数除法中能够向上取整。
	//	如果直接使用除以3的方式，对于某些不完全被3整除的值，将会导致向下取整，使得最终计算的结果偏小
	static constexpr const int INTERIOR_BITS = (BITS + 2) / 3;
	//	表示每个内部节点指针数组的长度
	static constexpr const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

	//	表示在叶子层级上应该消耗的位数
	static constexpr const int LEAF_BITS = BITS - (2 * INTERIOR_BITS);
	//	表示每个叶子节点值数组的长度
	static constexpr const int LEAF_LENGTH = 1 << LEAF_BITS;

	//	表示叶子节点覆盖的字节数
	static constexpr size_t LEAFCOVEREDBYTES = size_t{1} << (INTERIOR_BITS + PAGE_SHIFT);
	//	表示叶子节点所占用的巨大页面数
	static constexpr size_t LEAFHUGEPAGES = LEAFCOVEREDBYTES / HUGEPAGESIZE;

	//	基数树的叶节点
	struct Leaf
	{
		CompactSizeClass sizeclass[INTERIOR_LENGTH];
		Span* span[INTERIOR_LENGTH];
		void* value[LEAFHUGEPAGES];
		
	};

	//	基数树的内部节点
	struct Node
	{
		//Node* ptrs[INTERIOR_LENGTH];
		Leaf* leafs[INTERIOR_LENGTH];
	};

	//	基数树的根节点
	Node* root_[INTERIOR_LENGTH];
	size_t bytes_used_;

public:
	typedef uintptr_t Number;

	constexpr TCMalloc_PageMap3() : root_{}, bytes_used_(0)
	{
		//PreallocateMoreMemory();
	}

	//	用于在基数树中设置键值对，将给定的键k和值指针v存储在基数树中
	void set(Number k, Span* s)
	{
		//std::cout << "set传进来了" << std::endl;
		assert(k >> BITS == 0);
		
		//	这个就是一级索引，把低位移除，i1 表示从第 34 位到第 50 位的位段
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		//	这个就是二级索引， i2 表示的是从第 17 位到第 33 位的位置
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		//	这个就是三级索引，i3 表示从第 0 位到第 16 位的位段
		const Number i3 = k & (LEAF_LENGTH - 1);
		//std::cout << i1 << std::endl;
		//std::cout << i2 << std::endl;
		//std::cout << i3 << std::endl;

		//	用reinterpret_cast将根节点root_->ptrs[i1]->ptrs[i2]解释为Leaf*类型的指针，
		//	并访问其中的values数组，然后将给定的值指针v存储在该位置
		//	问题一： 这个位置必须先调用Ensure初始化后才能放进来
		root_[i1]->leafs[i2]->span[i3] = s;
		//std::cout << "set分配成功了" << std::endl;
	}

	void* get(Number k) const
	{
		//	这个就是一级索引，把低位移除，i1 表示从第 34 位到第 50 位的位段(最左边为高位)
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		//	这个就是二级索引， i2 表示的是从第 17 位到第 33 位的位置
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		//	这个就是三级索引，i3 表示从第 0 位到第 16 位的位段(最右边为低位)
		const Number i3 = k & (LEAF_LENGTH - 1);

		//	这里如果传进来的k的位数大于BITS，或者一级索引和二级索引里面为空，那就是没找到
		if ((k >> BITS) > 0 || root_[i1] == nullptr || root_[i1]->leafs[i2] == nullptr)
		{
			return nullptr;
		}
		return root_[i1]->leafs[i2]->span[i3];
	}

	//	用于确保在基数树中某个范围内的节点和叶子节点都被正确创建和分配内存
	//	start表示范围的起始值，n表示范围的大小（节点数）
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)
		{
			//	这个就是一级索引，把低位移除，i1 表示从第 34 位到第 50 位的位段(最左边为高位)
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
			//	这个就是二级索引， i2 表示的是从第 17 位到第 33 位的位置
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
			//std::cout << key << std::endl;
			//std::cout << i1 << std::endl;
			//std::cout << i2 << std::endl;

			//	检查索引i1和i2是否超出了合法范围，如果超出就返回false
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
			{
				return false;
			}

			//	如果根节点的一级索引对应的节点为NULL，则创建一个新的节点，
			//	并将其赋值给根节点的指针数组中相应的位置
			if (root_[i1] == nullptr)
			{
				Node* node = reinterpret_cast<Node*>(SystemAlloc_sbrk(sizeof(Node)));
				if (node == nullptr)
				{
					return false;
				}
				bytes_used_ += sizeof(Node);
				//std::cout << sizeof(Node) << std::endl;
				//std::cout << sizeof(*node) << std::endl;
				//std::cout << "memory前" << std::endl;
				memset(node, 0, sizeof(*node));
				//std::cout << "memory后" << std::endl;
				root_[i1] = node;
			}

			//	如果一级索引对应的节点的二级索引对应的叶子节点为NULL，
			//	则创建一个新的叶子节点，并将其赋值给一级索引对应的节点的指针数组中相应的位置
			if (root_[i1]->leafs[i2] == nullptr)
			{
				Leaf* leaf = reinterpret_cast<Leaf*>(SystemAlloc_sbrk(sizeof(Leaf)));
				if (leaf == nullptr)
				{
					return false;
				}
				bytes_used_ += sizeof(Leaf);
				//	使用reinterpret_cast将叶子节点指针转换为节点指针，并将其赋值给二级索引对应的位置
				memset(leaf, 0, sizeof(*leaf));
				root_[i1]->leafs[i2] = leaf;
			}

			//	更新key的值，使其跳过当前叶子节点所覆盖的范围
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	//	这个函数已作废，为了节省内存空间，Ensure应该在sbrk时调用，不能在创建pagemap时调用
	//	预分配足够的内存来跟踪所有可能的页面
	void PreallocateMoreMemory()
	{
		//Ensure(0, static_cast<size_t>(1) << BITS);
	}

};




#endif