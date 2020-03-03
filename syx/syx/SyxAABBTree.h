#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  struct AABBTreeContext;
  class AABBNode;

  class AABBNode {
  public:
    AABBNode(const AABBNode& other);
    AABBNode()
      : mParent(Null)
      , mHeight(Null)
      , mLeft(Null)
      , mRight(Null)
      , mIsLeaf(false) {
    }
    AABBNode& operator=(const AABBNode& other);
    void draw(const std::vector<AABBNode>& nodes) const;

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
    bool isLeaf() const {
      return mIsLeaf;
    }

    static const Handle Null = std::numeric_limits<size_t>::max();
  };

  class AABBTree: public Broadphase {
  public:
    AABBTree(int startingSize = 1000);
    ~AABBTree();

    Handle insert(const BoundingVolume& obj, void* userdata) override;
    void remove(Handle handle) override;
    void clear(void) override;

    Handle update(const BoundingVolume& newVol, Handle handle) override;

    void queryPairs(BroadphasePairContext& context) const override;
    void queryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const override;
    void queryVolume(const BoundingVolume& volume, BroadphaseContext& context) const override;
    //Maybe also have an interactive query for raycasting one at a time
    void draw(void) override;

    std::unique_ptr<BroadphaseContext> createHitContext() const override;
    std::unique_ptr<BroadphasePairContext> createPairContext() const override;
    bool isValid(const BroadphaseContext& conetxt) const override;
    bool isValid(const BroadphasePairContext& context) const override;

  private:
    inline const AABBNode& _getNode(Handle handle) const { return mNodes[handle]; }
    inline AABBNode& _getNode(Handle handle) { return mNodes[handle]; }

    void _insertNode(AABBNode& newNode, Handle newIndex);

    Handle _getHandle(const AABBNode& node) const;
    bool _evalEmpty(AABBTreeContext& context) const;
    void _pushToEval(Handle indexA, Handle indexB, AABBTreeContext& context) const;
    void _pushToEval(const AABBNode* nodeA, const AABBNode* nodeB, AABBTreeContext& context) const;
    std::pair<const AABBNode*, const AABBNode*> _popFromEval(AABBTreeContext& context) const;

    void _checkBounds(const AABBNode& nodeA, const AABBNode& nodeB, AABBTreeContext& context) const;
    void _traverseChild(const AABBNode& child, AABBTreeContext& context) const;
    void _leafBranchCase(const AABBNode& leaf, const AABBNode& branch, AABBTreeContext& context) const;
    void _drawHelper(Handle index);

    void _getCollidingHelper(Handle curNode, const BoundingVolume& volume, AABBTreeContext& context) const;

    void _syncParents(Handle index);
    void _freeNode(Handle index);
    Handle _createNode();
    void _constructFreeList(unsigned start = 0);
    //Returns root from given index. Will be index if no rotation was needed
    Handle _balance(Handle index);
    //Uses rotation algorithm presented by Erin Catto's Box2d
    Handle _balance(Handle index, int balance);
    //Returns cost of traversing down to "traversal" while inserting "insertBox"
    float _traversalCost(AABBNode& traversal, AABB& insertBox);

    Handle mRoot;
    Handle mFreeList;
    std::vector<AABBNode> mNodes;

    static float sBoxPadding;
  };
}
