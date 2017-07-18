#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  class AABBNode {
  public:
    AABBNode(const AABBNode& other);
    AABBNode(void): mParent(Null), mHeight(Null), mLeft(Null), mRight(Null), mIsLeaf(false) {}
    AABBNode& operator=(const AABBNode& other);
    void AABBNode::Draw(const std::vector<AABBNode>& nodes) const;

    //For unused nodes, they will be in the free list, using next to point to next free one
    union {
      Handle mParent;
      Handle mNext;
    };
    //Null height means node is free
    int mHeight;

    //Union because only leaf nodes hold data
    DISABLE_WARNING_START(4201)
    union {
      struct {
        Handle mLeft;
        Handle mRight;
      };
      ResultNode mLeafData;
    };
    DISABLE_WARNING_END

    bool mTraversed;
    bool mIsLeaf;
    AABB mAABB;
    //Since right doesn't use the same memory as m_obj, it will always be Null on a leaf
    bool IsLeaf(void) const {
      return mIsLeaf;
    }

    static const Handle Null = std::numeric_limits<size_t>::max();
  };

  struct AABBTreeContext : public BroadphaseContext {
    //Used during raycasting to keep track of things to cast against, stored here to save on re-allocations
    std::queue<const AABBNode*> mNodeQueue;
    //Number of elements in use in m_toEvaluate
    size_t mEvalSize;
    std::vector<std::pair<const AABBNode*, const AABBNode*>> mToEvaluate;
    SmallIndexSet<100> mTraversed;
  };

  class AABBTree: public Broadphase {
  public:
    AABBTree(int startingSize = 1000)
      : mRoot(AABBNode::Null)
      , mFreeList(AABBNode::Null) {
      mNodes.resize(startingSize); ConstructFreeList();
    }

    Handle Insert(const BoundingVolume& obj, void* userdata) override;
    void Remove(Handle handle) override;
    void Clear(void) override;

    Handle Update(const BoundingVolume& newVol, Handle handle) override;

    void QueryPairs(BroadphaseContext& context) const override;
    void QueryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const override;
    void QueryVolume(const BoundingVolume& volume, BroadphaseContext& context) const override;
    //Maybe also have an interactive query for raycasting one at a time
    void AABBTree::Draw(void) override;

  private:
    inline const AABBNode& GetNode(Handle handle) const { return mNodes[handle]; }
    inline AABBNode& GetNode(Handle handle) { return mNodes[handle]; }

    void InsertNode(AABBNode& newNode, Handle newIndex);

    Handle GetHandle(const AABBNode& node) const;
    bool EvalEmpty(AABBTreeContext& context) const;
    void PushToEval(Handle indexA, Handle indexB, AABBTreeContext& context) const;
    void PushToEval(const AABBNode* nodeA, const AABBNode* nodeB, AABBTreeContext& context) const;
    std::pair<const AABBNode*, const AABBNode*> PopFromEval(AABBTreeContext& context) const;

    void CheckBounds(const AABBNode& nodeA, const AABBNode& nodeB, AABBTreeContext& context) const;
    void TraverseChild(const AABBNode& child, AABBTreeContext& context) const;
    void LeafBranchCase(const AABBNode& leaf, const AABBNode& branch, AABBTreeContext& context) const;
    void DrawHelper(Handle index);

    void GetCollidingHelper(Handle curNode, const BoundingVolume& volume, AABBTreeContext& context) const;

    void SyncParents(Handle index);
    void FreeNode(Handle index);
    Handle CreateNode(void);
    void ConstructFreeList(unsigned start = 0);
    //Returns root from given index. Will be index if no rotation was needed
    Handle Balance(Handle index);
    //Uses rotation algorithm presented by Erin Catto's Box2d
    Handle Balance(Handle index, int balance);
    //Returns cost of traversing down to "traversal" while inserting "insertBox"
    float TraversalCost(AABBNode& traversal, AABB& insertBox);

    Handle mRoot;
    Handle mFreeList;
    std::vector<AABBNode> mNodes;

    static float sBoxPadding;
  };
}
