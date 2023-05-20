#include "common.hpp"

#include <cstring>

 static constexpr int kLeafBits = (35 + 2) / 3;  // Round up
  static constexpr int kLeafLength = 1 << kLeafBits;
  static constexpr int kMidBits = (35 + 2) / 3;  // Round up
  static constexpr int kMidLength = 1 << kMidBits;
  static constexpr int kRootBits = 35 - kLeafBits - kMidBits;
  static_assert(kRootBits > 0, "Too many bits assigned to leaf and mid");
  // (1<<kRootBits) must not overflow an "int"
  static_assert(kRootBits < sizeof(int) * 8 - 1, "Root bits too large");
  static constexpr int kRootLength = 1 << kRootBits;

  struct Leaf {
    // We keep parallel arrays indexed by page number.  One keeps the
    // size class; another span pointers; the last hugepage-related
    // information.  The size class information is kept segregated
    // since small object deallocations are so frequent and do not
    // need the other information kept in a Span.
    CompactSizeClass sizeclass[kLeafLength];
    Span* span[kLeafLength];

  };

	//	基数树的内部节点
	struct Node
	{
		//Node* ptrs[INTERIOR_LENGTH];
		Leaf* leafs[kLeafLength];
	};

	//	基数树的根节点
	Node* root_[kLeafLength];
	size_t bytes_used_;


  	bool Ensure(size_t start, uint64_t n)
	{
    std::cout << "进来了" << std::endl;

  }

int main() 
{
    Node* node = reinterpret_cast<Node*>(SystemAlloc_sbrk(sizeof(Node)));
    memset(node, 0, sizeof(*node));

  Ensure(0, static_cast<uint64_t>(1) << 35);
  std::cout << sizeof(size_t) << std::endl;
    return 0;
}