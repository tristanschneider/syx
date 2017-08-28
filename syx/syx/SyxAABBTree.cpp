#include "Precompile.h"
#include "SyxAABBTree.h"

namespace Syx {
  float AABBTree::sBoxPadding = 0.0f;

  AABBNode::AABBNode(const AABBNode& other) {
    *this = other;
  }

  AABBNode& AABBNode::operator=(const AABBNode& other) {
    //Copy entire union over, union's biggest thing is ResultNode
    memcpy(&mLeft, &other.mLeft, sizeof(ResultNode));
    mHeight = other.mHeight;
    mAABB = other.mAABB;
    mParent = other.mParent;
    mIsLeaf = other.mIsLeaf;
    return *this;
  }

  void AABBTree::_syncParents(Handle index) {
    while(index != AABBNode::Null) {
      if(!mNodes[index].isLeaf()) {
        index = _balance(index);

        Handle left = mNodes[index].mLeft;
        Handle right = mNodes[index].mRight;

        mNodes[index].mHeight = 1 + std::max(mNodes[left].mHeight, mNodes[right].mHeight);
        mNodes[index].mAABB = AABB::combined(mNodes[left].mAABB, mNodes[right].mAABB);
      }

      index = mNodes[index].mParent;
    }
  }

  void AABBTree::_freeNode(Handle index) {
    AABBNode* toFree = &mNodes[index];
    toFree->mHeight = AABBNode::Null;
    toFree->mNext = mFreeList;
    toFree->mRight = AABBNode::Null;
    mFreeList = index;
  }

  Handle AABBTree::_createNode(void) {
    //Grow node list if necessary
    if(mNodes[mFreeList].mNext == AABBNode::Null) {
      mNodes.resize(mNodes.size()*2);
      _constructFreeList(mFreeList);
    }

    Handle freeNode = mFreeList;
    mFreeList = mNodes[mFreeList].mNext;
    return freeNode;
  }

  void AABBTree::_constructFreeList(unsigned start) {
    //Should never happen, but would cause a crash without the check from the -1u below
    if(mNodes.empty())
      return;

    for(unsigned i = start; i < mNodes.size() - 1u; ++i) {
      mNodes[i].mNext = i + 1;
      mNodes[i].mHeight = AABBNode::Null;
    }

    mNodes.back().mNext = AABBNode::Null;

    if(mFreeList == AABBNode::Null)
      mFreeList = start;
  }

  Handle AABBTree::_balance(Handle index) {
    if(mNodes[index].isLeaf() || mNodes[index].mHeight < 2)
      return index;
    return _balance(index, mNodes[mNodes[index].mRight].mHeight - mNodes[mNodes[index].mLeft].mHeight);
  }

  Handle AABBTree::_balance(Handle index, int balance) {
    //Tree is already balanced, don't need to do anything
    if(balance >= -1 && balance <= 1)
      return index;
    //balance = height(A->right) - height(A->left)
    //index = A
    //B is node being promoted
    //D and E are going to be moved
    //C is child of A that isn't being promoted
    //A has other children that aren't shown because they aren't directly moved
    //   A   OR   A
    // B   C    C   B
    //D E          D E
    //^-Balance   ^+Balance
    Handle aIndex = index;
    AABBNode& aNode = mNodes[aIndex];
    Handle bIndex = aNode.mRight;
    Handle cIndex = aNode.mLeft;
    if(balance < 0)
      std::swap(bIndex, cIndex);
    AABBNode& bNode = mNodes[bIndex];
    AABBNode& cNode = mNodes[cIndex];

    Handle dIndex = bNode.mLeft;
    Handle eIndex = bNode.mRight;
    AABBNode& dNode = mNodes[dIndex];
    AABBNode& eNode = mNodes[eIndex];

    //Promote B
    bNode.mLeft = aIndex;
    bNode.mParent = aNode.mParent;
    aNode.mParent = bIndex;

    //Point A's old parent at B
    if(bNode.mParent != AABBNode::Null) {
      if(mNodes[bNode.mParent].mLeft == aIndex)
        mNodes[bNode.mParent].mLeft = bIndex;
      else
        mNodes[bNode.mParent].mRight = bIndex;
    }
    else
      mRoot = bIndex;

    //Deal with B's children
    //This could be factored to a function, as only d and e are swapped
    if(dNode.mHeight > eNode.mHeight) {
      bNode.mRight = dIndex;
      if(balance < 0)
        aNode.mLeft = eIndex;
      else
        aNode.mRight = eIndex;
      eNode.mParent = aIndex;

      aNode.mAABB = AABB::combined(cNode.mAABB, eNode.mAABB);
      bNode.mAABB = AABB::combined(aNode.mAABB, dNode.mAABB);

      aNode.mHeight = 1 + std::max(cNode.mHeight, eNode.mHeight);
      bNode.mHeight = 1 + std::max(aNode.mHeight, dNode.mHeight);
    }
    else {
      bNode.mRight = eIndex;
      if(balance < 0)
        aNode.mLeft = dIndex;
      else
        aNode.mRight = dIndex;
      dNode.mParent = aIndex;

      aNode.mAABB = AABB::combined(cNode.mAABB, dNode.mAABB);
      bNode.mAABB = AABB::combined(aNode.mAABB, eNode.mAABB);

      aNode.mHeight = 1 + std::max(cNode.mHeight, dNode.mHeight);
      bNode.mHeight = 1 + std::max(aNode.mHeight, eNode.mHeight);
    }

    return bIndex;
  }

  Handle AABBTree::insert(const BoundingVolume& obj, void* userdata) {
    Handle newIndex = _createNode();
    AABBNode& newNode = mNodes[newIndex];
    newNode.mLeafData.mHandle = newIndex;
    newNode.mLeafData.mUserdata = userdata;
    newNode.mAABB = obj.mAABB;
    newNode.mAABB.pad(sBoxPadding);
    newNode.mHeight = 0;
    newNode.mIsLeaf = true;

    _insertNode(newNode, newIndex);
    return newIndex;
  }

  void AABBTree::_insertNode(AABBNode& newNode, Handle newIndex) {
    if(mRoot == AABBNode::Null) {
      newNode.mParent = AABBNode::Null;
      mRoot = newIndex;
      return;
    }

    AABB& newBox = newNode.mAABB;

    //Find best sibling to pair aabb with based on surface area heuristic
    Handle curIndex = mRoot;
    while(!mNodes[curIndex].isLeaf()) {
      AABBNode& curNode = mNodes[curIndex];
      AABBNode& right = mNodes[curNode.mRight];
      AABBNode& left = mNodes[curNode.mLeft];

      float curArea = curNode.mAABB.getSurfaceArea();

      float combinedArea = AABB::combined(curNode.mAABB, newBox).getSurfaceArea();
      //Minimum cost of inserting node lower in the tree
      float inheritanceCost = 2.0f*(combinedArea - curArea);
      //Cost of creating a new parent for this node and new node
      float parentCost = 2.0f*combinedArea;

      float leftCost = _traversalCost(left, newBox) + inheritanceCost;
      float rightCost = _traversalCost(right, newBox) + inheritanceCost;

      //If it is cheaper to create a new parent than descend
      if(parentCost < rightCost && parentCost < leftCost)
        break;

      //Traverse down the cheaper path
      if(rightCost < leftCost)
        curIndex = curNode.mRight;
      else
        curIndex = curNode.mLeft;
    }

    //Sibling was found in above loop upon break
    Handle sibling = curIndex;
    Handle oldParent = mNodes[sibling].mParent;
    Handle newParent = _createNode();
    AABBNode& newParRef = mNodes[newParent];

    newParRef.mParent = oldParent;
    newParRef.mAABB = AABB::combined(mNodes[sibling].mAABB, newBox);
    newParRef.mHeight = mNodes[sibling].mHeight + 1;
    newParRef.mIsLeaf = false;

    //If the sibling was the root
    if(oldParent == AABBNode::Null)
      mRoot = newParent;
    else//If the sibling wasn't the root
    {
      //See if new parent belongs on right or left and put it there
      if(mNodes[oldParent].mRight == sibling)
        mNodes[oldParent].mRight = newParent;
      else
        mNodes[oldParent].mLeft = newParent;
    }

    mNodes[newParent].mLeft = sibling;
    mNodes[newParent].mRight = newIndex;
    mNodes[sibling].mParent = newParent;
    newNode.mParent = newParent;

    //Balances and updates aabbs of all parents
    _syncParents(newParent);
  }

  float AABBTree::_traversalCost(AABBNode& traversal, AABB& insertBox) {
    float result;
    AABB combined = AABB::combined(insertBox, traversal.mAABB);
    float combinedArea = combined.getSurfaceArea();

    if(traversal.isLeaf())
      result = combinedArea;
    else
      result = combinedArea - traversal.mAABB.getSurfaceArea();

    return result;
  }

  void AABBTree::remove(Handle toRemove) {
    //It isn't in the broadphase, so don't do anything
    if(toRemove == AABBNode::Null)
      return;
    //Remove object and replace its parent with its sibling
    Handle parent = mNodes[toRemove].mParent;

    _freeNode(toRemove);

    if(parent == AABBNode::Null) {
      mRoot = AABBNode::Null;
      return;
    }

    Handle grandParent = mNodes[parent].mParent;

    //Get sibling
    Handle sibling = mNodes[parent].mLeft;
    if(sibling == toRemove)
      sibling = mNodes[parent].mRight;

    //Connect sibling to grandparent
    mNodes[sibling].mParent = grandParent;
    if(grandParent == AABBNode::Null) {
      mRoot = sibling;
      _freeNode(parent);
      return;
    }
    else if(mNodes[grandParent].mLeft == parent)
      mNodes[grandParent].mLeft = sibling;
    else
      mNodes[grandParent].mRight = sibling;

    //Kill disconnected parent
    _freeNode(parent);

    _syncParents(grandParent);
  }

  Handle AABBTree::update(const BoundingVolume& newVol, Handle handle) {
    bool needsUpdate = handle == AABBNode::Null;
    AABBNode* node = nullptr;
    if(!needsUpdate) {
      node = &_getNode(handle);
      needsUpdate = !node->mAABB.isInside(newVol.mAABB);
    }

    if(needsUpdate && node) {
      void* userdata = node->mLeafData.mUserdata;
      remove(handle);
      return insert(newVol, userdata);
    }

    return handle;
  }

  void AABBTree::clear(void) {
    mRoot = mFreeList = AABBNode::Null;
    _constructFreeList();
  }

  void AABBTree::_checkBounds(const AABBNode& nodeA, const AABBNode& nodeB, AABBTreeContext& context) const {
    if(nodeA.mAABB.overlapping(nodeB.mAABB)) {
      ResultNode resultA(nodeA.mLeafData.mHandle, nodeA.mLeafData.mUserdata);
      ResultNode resultB(nodeB.mLeafData.mHandle, nodeB.mLeafData.mUserdata);
      context.mQueryPairResults.push_back({resultA, resultB});
    }
  }

  bool AABBTree::_evalEmpty(AABBTreeContext& context) const {
    return context.mEvalSize == 0;
  }

  void AABBTree::_pushToEval(Handle indexA, Handle indexB, AABBTreeContext& context) const {
    _pushToEval(&_getNode(indexA), &_getNode(indexB), context);
  }

  void AABBTree::_pushToEval(const AABBNode* nodeA, const AABBNode* nodeB, AABBTreeContext& context) const {
    //First clause is to avoid computing sibling intersections, because they shouldn't be
    //While adding intersection computations, this has the potential of cutting off entire subtrees, so is worth it
    if(nodeA->mParent != nodeB->mParent && !nodeA->mAABB.overlapping(nodeB->mAABB))
      return;
    //+1 for when the size is 0
    if(context.mEvalSize >= context.mToEvaluate.size())
      context.mToEvaluate.resize(2*context.mEvalSize + 1);
    context.mToEvaluate[context.mEvalSize++] = std::pair<const AABBNode*, const AABBNode*>(nodeA, nodeB);
  }

  std::pair<const AABBNode*, const AABBNode*> AABBTree::_popFromEval(AABBTreeContext& context) const {
    return context.mToEvaluate[--context.mEvalSize];
  }

  void AABBTree::_traverseChild(const AABBNode& child, AABBTreeContext& context) const {
    if(context.mTraversed.insert(_getHandle(child)))
      return;
    _pushToEval(child.mLeft, child.mRight, context);
  }

  void AABBTree::_leafBranchCase(const AABBNode& leaf, const AABBNode& branch, AABBTreeContext& context) const {
    _traverseChild(branch, context);
    _pushToEval(&leaf, &_getNode(branch.mLeft), context);
    _pushToEval(&leaf, &_getNode(branch.mRight), context);
  }

  Handle AABBTree::_getHandle(const AABBNode& node) const {
    return static_cast<size_t>(&node - &mNodes[0]);
  }

  void AABBTree::queryPairs(BroadphaseContext& context) const {
    AABBTreeContext& c = static_cast<AABBTreeContext&>(context);
    c.mQueryPairResults.clear();
    if(mRoot == AABBNode::Null || mNodes[mRoot].isLeaf())
      return;

    c.mTraversed.clear();
    _pushToEval(mNodes[mRoot].mLeft, mNodes[mRoot].mRight, c);

    while(!_evalEmpty(c)) {
      std::pair<const AABBNode*, const AABBNode*> pair = _popFromEval(c);
      if(pair.first->isLeaf()) {
        if(pair.second->isLeaf())
          _checkBounds(*pair.first, *pair.second, c);
        else
          _leafBranchCase(*pair.first, *pair.second, c);
      }
      else if(pair.second->isLeaf())
        _leafBranchCase(*pair.second, *pair.first, c);
      else {
        _traverseChild(*pair.first, c);
        _traverseChild(*pair.second, c);
        _pushToEval(pair.first->mLeft, pair.second->mLeft, c);
        _pushToEval(pair.first->mLeft, pair.second->mRight, c);
        _pushToEval(pair.first->mRight, pair.second->mLeft, c);
        _pushToEval(pair.first->mRight, pair.second->mRight, c);
      }
    }
  }

  void AABBTree::queryVolume(const BoundingVolume& volume, BroadphaseContext& context) const {
    AABBTreeContext& c = static_cast<AABBTreeContext&>(context);
    c.mQueryResults.clear();

    _getCollidingHelper(mRoot, volume, c);
  }

  void AABBTree::_getCollidingHelper(Handle curNode, const BoundingVolume& volume, AABBTreeContext& context) const {
    if(curNode == AABBNode::Null)
      return;
    const AABBNode& node = _getNode(curNode);
    //Compare with the object's aabb, which is smaller, if this is a leaf, otherwise, node aabb
    if(node.mAABB.overlapping(volume.mAABB)) {
      if(node.isLeaf())
        context.mQueryResults.push_back(ResultNode(node.mLeafData.mHandle, node.mLeafData.mUserdata));
      else {
        _getCollidingHelper(node.mLeft, volume, context);
        _getCollidingHelper(node.mRight, volume, context);
      }
    }
  }

  void AABBTree::queryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const {
    AABBTreeContext& c = static_cast<AABBTreeContext&>(context);
    c.mQueryResults.clear();
    if(mRoot != AABBNode::Null)
      c.mNodeQueue.push(&mNodes[mRoot]);

    while(!c.mNodeQueue.empty()) {
      const AABBNode& curNode = *c.mNodeQueue.front();
      c.mNodeQueue.pop();

      const AABB& aabb = curNode.mAABB;
      float t;
      if(aabb.lineIntersect(start, end, &t)) {
        if(curNode.isLeaf())
          c.mQueryResults.push_back(ResultNode(curNode.mLeafData.mHandle, curNode.mLeafData.mUserdata));
        else//is branch
        {
          c.mNodeQueue.push(&mNodes[curNode.mLeft]);
          c.mNodeQueue.push(&mNodes[curNode.mRight]);
        }
      }
    }
  }

  void AABBTree::_drawHelper(Handle index) {
    DebugDrawer& d = DebugDrawer::get();
    if(index == AABBNode::Null)
      return;
    const AABBNode& node = mNodes[index];
    if(node.isLeaf())
      d.setColor(1.0f, 0.0f, 0.0f);
    else
      d.setColor(0.0f, 0.0f, 1.0f);
    node.draw(mNodes);
    if(!node.isLeaf()) {
      _drawHelper(node.mLeft);
      _drawHelper(node.mRight);
    }
  }

  void AABBTree::draw(void) {
    DebugDrawer::get().setColor(0.0f, 0.0f, 1.0f);
    if(mRoot != AABBNode::Null)
      mNodes[mRoot].draw(mNodes);
    _drawHelper(mRoot);
  }

  void AABBNode::draw(const std::vector<AABBNode>& nodes) const {
    DebugDrawer& d = DebugDrawer::get();
    d.drawCube(mAABB.getCenter(), mAABB.getDiagonal(), Vec3::UnitX, Vec3::UnitY);
    Vec3 center = mAABB.getCenter();
    d.drawLine(center, mAABB.getMax());
    if(!isLeaf()) {
      d.drawVector(center, nodes[mRight].mAABB.getCenter() - center);
      d.drawVector(center, nodes[mLeft].mAABB.getCenter() - center);
    }
  }
}