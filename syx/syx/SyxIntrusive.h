#pragma once
#include "SyxInterface.h"

//This macro must be used in classes that want to be in intrusive structures
#define DeclareIntrusiveNode(ClassType) IntrusiveNode<ClassType> mIntrusiveNode;
//#define VECLIST_TRAVERSAL_TRACKING

namespace Syx {
  //Since this is intrusive, you need to be careful about the assignment operator,
  //as information about the list will be copied as well.
  template<typename T>
  class IntrusiveNode {
  public:
    IntrusiveNode* mNext;
    IntrusiveNode* mPrev;
    T* mOwner;
    IntrusiveNode()
      : mNext(0)
      , mPrev(0)
      , mOwner(0) {
    }
    inline bool isInList() { return mOwner != nullptr; }
    inline void initialize() { mNext = mPrev = nullptr; mOwner = nullptr; }
    //Could have destructor unhook it, but I think that should be owner's responsibility
    virtual ~IntrusiveNode() {}
  };

  template <typename T>
  class IntrusiveIterator {
  public:
    IntrusiveIterator(T* node)
      : mPtr(node ? &node->mIntrusiveNode : 0) {
    }
    IntrusiveIterator()
      : mPtr(nullptr) {
    }
    T& operator*() { return *mPtr->mOwner; }
    T* dataPointer() { return mPtr->mOwner; }
    T* operator->() { return dataPointer(); }
    //Pre
    IntrusiveIterator& operator++(void) { mPtr = mPtr->mNext; return *this; }
    //Post
    IntrusiveIterator operator++(int) { IntrusiveIterator result(*this); ++(*this); return result; }
    IntrusiveIterator& operator--(void) { mPtr = mPtr->mPrev; return *this; }
    IntrusiveIterator operator--(int) { IntrusiveIterator result(*this); --(*this); }
    bool operator==(const IntrusiveIterator& rhs) const { return mPtr == rhs.mPtr; }
    bool operator!=(const IntrusiveIterator& rhs) const { return mPtr != rhs.mPtr; }
    IntrusiveIterator& operator=(IntrusiveNode<T>* node) { mPtr = node; }
  private:
    IntrusiveNode<T>* mPtr;
  };

  //Intrusive Linked List. Objects must use DECLARE_INTRUSIVE_NODE macro in public section
  //Objects added to list are not copied, they
  //are just pointed to ther neighbors, so they must exist in a scope
  //for which that makes sense
  template <typename T>
  class IntrusiveList {
  public:
    IntrusiveList()
      : mHead(nullptr)
      , mTail(nullptr)
      , mSize(0) {
    }
    ~IntrusiveList() { mHead = nullptr; mTail = nullptr; }
    //I don't support copying these, just doing this so mutex compiles
    IntrusiveList(const IntrusiveList&)
      : mHead(nullptr)
      , mTail(nullptr)
      , mSize(0) {
    }
    IntrusiveList& operator=(const IntrusiveList&) { mHead = nullptr; mTail = nullptr; return *this; }

    IntrusiveIterator<T> begin() { return IntrusiveIterator<T>(mHead); }
    IntrusiveIterator<T> end() { return sEnd; }
    IntrusiveIterator<T> begin() const { return IntrusiveIterator<T>(mHead); }
    IntrusiveIterator<T> end() const { return sEnd; }

    T* front() { return mHead; }
    T* back() { return mTail; }
    bool empty() { return mHead == nullptr; }
    //User is responsible for freeing memory
    void clear() { mHead = mTail = nullptr; mSize = 0; }
    size_t size() { return mSize; }

    void lock() { mMutex.lock(); }
    void unlock() { mMutex.unlock(); }

    void link(T* prev, T* next) {
      prev->mIntrusiveNode.mNext = &next->mIntrusiveNode;
      next->mIntrusiveNode.mPrev = &prev->mIntrusiveNode;
    }

    void link(T* prev, T* middle, T* next) {
      link(prev, middle);
      link(middle, next);
    }

    void pushFront(T* node) {
      if(mHead)
        link(node, mHead);
      mHead = node;
      if(!mTail)
        mTail = node;
      node->mIntrusiveNode.mOwner = node;
      ++mSize;
    }

    void pushBack(T* node) {
      if(mTail)
        link(mTail, node);
      mTail = node;
      if(!mHead)
        mHead = node;
      node->mIntrusiveNode.mOwner = node;
      ++mSize;
    }

    void insertAfter(T* obj, T* before) {
      if(mTail == before)
        pushBack(obj);
      else {
        link(before, obj, before->mIntrusiveNode.mNext->mOwner);
        ++mSize;
        obj->mIntrusiveNode.mOwner = obj;
      }
    }

    void insertBefore(T* obj, T* after) {
      if(mHead == after)
        pushFront(obj);
      else {
        link(after->mIntrusiveNode.mPrev->mOwner, obj, after);
        obj->mIntrusiveNode.mOwner = obj;
        ++mSize;
      }
    }

    void remove(T* obj) {
      IntrusiveNode<T>* node = &obj->mIntrusiveNode;
      //If this isn't the case, then this object wasn't in the list, so we wouldn't want to decrement
      if(node->isInList())
        --mSize;

      if(node->mPrev)
        node->mPrev->mNext = node->mNext;
      else
        mHead = node->mNext ? node->mNext->mOwner : nullptr;
      if(node->mNext)
        node->mNext->mPrev = node->mPrev;
      else
        mTail = node->mPrev ? node->mPrev->mOwner : nullptr;

      node->mPrev = node->mNext = nullptr;
      node->mOwner = nullptr;
    }

  private:
    std::mutex mMutex;
    size_t mSize;
    T* mHead;
    T* mTail;

    static const IntrusiveIterator<T> sEnd;
  };

  template<typename T>
  const IntrusiveIterator<T> IntrusiveList<T>::sEnd;

  //T must use the DECLARE_INTRUSIVE_NODE macro in a public section
  //A list stored contiguously in memory, guaranteed not to move.
  //Useful if pre-allocation and random deletion is important.
  //Order must not be important, as control over it is not exposed
  template<typename T>
  class VecList {
  public:
    VecList(size_t size = 100)
      : mPageSize(size)
      , mInUse(0)
#ifdef VECLIST_TRAVERSAL_TRACKING
      , mNoOfSamples(0)
      , mTraversalSum(0)
#endif
    {
    }

    //Nothing uses this structure that would want it to copy, just doing this so it compiles with a mutex
    VecList(const VecList& rhs) { *this = rhs; }

    size_t size() { return mInUse; }

    //This is dangerous and only works because my uses of this structure don't care about a proper copy. Should probably revisit later
    VecList& operator=(const VecList& rhs) {
      if(&rhs != this) {
        deletePages();
#ifdef VECLIST_TRAVERSAL_TRACKING
        mNoOfSamples = 0;
        mTraversalSum = 0;
#endif
        mInUse = 0;
        mObjects = rhs.mObjects;
        mFree = rhs.mFree;
        mPageSize = rhs.mPageSize;
        //This would need to be fixed if I wanted a proper copy.
        /*if(rhs.m_pages && rhs.m_inUse)
            for(auto it = rhs.Begin(); it != rhs.End(); ++it)
              PushBack(*it.DataPointer());*/
      }
      return *this;
    }

    void deletePages() {
      for(auto it = begin(); it != end();) {
        T& obj = *it;
        ++it;
        obj.~T();
      }
      //delete the pages themselves
      for(uint8_t* page : mPagePool) {
        AlignedFree(page);
      }
      //delete the pointers to the pages
      mPagePool.clear();
    }

    ~VecList() {
      deletePages();
    }

    void initializePage(uint8_t* page) {
      std::memset(page, 0, mPageSize * sizeof(T));
      for(size_t i = 0; i < mPageSize; ++i)
        mFree.pushBack(reinterpret_cast<T*>(&page[i * sizeof(T)]));
    }

    bool empty() { return mInUse == 0; }

    T* front() { return mObjects.front(); }
    T* back() { return mObjects.back(); }
    IntrusiveIterator<T> begin() { return mObjects.begin(); }
    IntrusiveIterator<T> end() { return mObjects.end(); }
    IntrusiveIterator<T> begin() const { return mObjects.begin(); }
    IntrusiveIterator<T> end() const { return mObjects.end(); }
    //Gives you a new node and automatically pushes it to front
    //Must be careful when assigning, so as not to overwrite next and prev
    T* getToFront() {
      T* newNode = get();
      addToFront(newNode);
      return newNode;
    }

    //Gives you a new node and automatically pushes it back
    T* getToBack() {
      T* newNode = get();
      addToBack(newNode);
      return newNode;
    }

    //Creates a copy of given object and adds it to front of list
    bool pushFront(const T& obj) {
      T* newNode = get();
      *newNode = obj;
      return addToFront(newNode);
    }

    //Creates a copy of given object and adds it to back of list
    bool pushBack(const T& obj) {
      T* newNode = get();
      *newNode = obj;
      return addToBack(newNode);
    }

    //Push in a way that optimizes for cache coherency and assumes you don't care about order
    T* push(const T& obj) {
      T* newNode = get();
      *newNode = obj;
      if(mObjects.empty()) {
        addToBack(newNode);
        return back();
      }

      T* leftNeighbor = nullptr;
      //If this is the first in the page, (It's not of the first page because list isn't empty)
      //Use the last index of the previous page. This is guaranteed to be a used node because
      //the free list is sorted low to high by address
      if(isPageHead(*newNode)) {
        int page = getPage(*newNode);
        //On first page, belongs as new head of list
        if(!page) {
          mObjects.pushFront(newNode);
          return newNode;
        }
        //Get last index of previous list
        leftNeighbor = &getObjectAtIndex(page - 1, int(mPageSize) - 1);
      }
      else
        leftNeighbor = newNode - 1;
      //Insert it right next to its left neighbor so looping is contiguous
      mObjects.insertAfter(newNode, leftNeighbor);
      return newNode;
    }

    //Remove object from list
    void freeObj(T* toFree) {
      mObjects.remove(toFree);
      //Insert in a way that ensures free list is sorted from lowest address to highest
      if(mFree.empty())
        mFree.pushBack(toFree);
      else {
        T* leftNeighbor = findFreeNeighbor(toFree);
        if(leftNeighbor)
          mFree.insertAfter(toFree, leftNeighbor);
        //If none was found, there's nothing to push after because it should be first
        else
          mFree.pushFront(toFree);
      }
      --mInUse;

      //Call destructor but preserve intrusive values
      auto temp = toFree->mIntrusiveNode;
      toFree->~T();
      toFree->mIntrusiveNode = temp;
    }

    void freeObj(IntrusiveIterator<T> it) {
      freeObj(it.dataPointer());
    }

    void clear() {
      T* curNode = mObjects.front();
      while(curNode) {
        freeObj(curNode);
        curNode = mObjects.front();
      }
    }

    //For debugging cache coherency
    void printIndexes() {
      std::string s;
      for(auto it = mObjects.begin(); it != mObjects.end(); ++it) {
        int page = getPage(*it);
        if(page == -1)
          s += "(X,X)";
        else
          s += "(" + std::to_string(page) + "," + std::to_string(getIndexInPage(*it, page)) + ")";
      }
      Interface::log(s);
    }

#ifdef VECLIST_TRAVERSAL_TRACKING
    float getTraversalAverage() {
      return static_cast<float>(mTraversalSum)/static_cast<float>(mNoOfSamples);
    }
#endif
  private:
    void addPage() {
      //Put new page in last slot
      mPagePool.push_back(static_cast<uint8_t*>(Interface::allocAligned(sizeof(T)*mPageSize)));

      //Add new page to free list
      initializePage(mPagePool.back());
    }

    //Get a node from the free pool to add to your list (calling this does not add it to anything)
    T* get() {
      if(mFree.empty())
        addPage();
      T* newNode = mFree.front();

      //Now that the object is going to be used, call its constructor, but be sure to preserve the intrusive node value
      auto temp = newNode->mIntrusiveNode;
      new (newNode) T();
      newNode->mIntrusiveNode = temp;

      mFree.remove(newNode);
      ++mInUse;
      return newNode;
    }

    bool addToFront(T* obj) {
      mObjects.pushFront(obj);
      return true;
    }

    bool addToBack(T* obj) {
      mObjects.pushBack(obj);
      return true;
    }

    //When obj is beeing freed, this is called to find the node it should 
    //be inserted after in the free list to ensure addresses are sorted low to high
    T* findFreeNeighbor(T* obj) {
      //Since it is being removed, it shouldn't be possible for it not to be on a page
      int objPage = getPage(*obj);
      //Worst case for this is to have to traverse entire free list when removing left to right.
      //Mitigate this by checking tail first. Now worst is removing second from last after a long free block
      T* back = mFree.back();
      int backPage = getPage(*back);
      if(backPage < objPage || (backPage == objPage && mFree.back() < obj))
        return mFree.back();

      //This is returned if obj belongs at the head
      T* lastObj = nullptr;
#ifdef VECLIST_TRAVERSAL_TRACKING
      unsigned traversals = 0;
#endif
      for(auto it = mFree.begin(); it != mFree.end(); ++it) {
        T* curObj = it.dataPointer();
        int curPage = getPage(*curObj);
        if(curPage > objPage || (curPage == objPage && curObj > obj)) {
#ifdef VECLIST_TRAVERSAL_TRACKING
          ++mNoOfSamples;
          mTraversalSum += traversals;
#endif
          return lastObj;
        }

        lastObj = curObj;
#ifdef VECLIST_TRAVERSAL_TRACKING
        ++traversals;
#endif
      }
      //Should never happen
      return nullptr;
    }

    size_t getIndexInPage(const T& obj, int page) const {
      //Caller's responsibility to make sure it is actually within this page
      return (static_cast<size_t>(reinterpret_cast<const uint8_t*>(&obj) - mPagePool[page])) / sizeof(T);
    }

    T& getObjectAtIndex(int page, int index) {
      return *reinterpret_cast<T*>(mPagePool[page][index * sizeof(T)]);
    }

    int getPage(const T& obj) {
      uintptr_t objAddr = reinterpret_cast<uintptr_t>(&obj);
      for(size_t i = 0; i < mPagePool.size(); ++i) {
        const uint8_t* page = mPagePool[i];
        //If object is in this page
        if(objAddr >= reinterpret_cast<uintptr_t>(page) && objAddr <= reinterpret_cast<uintptr_t>(page) + mPageSize*sizeof(T)) {
          return static_cast<int>(i);
        }
      }
      return -1;
    }

    bool isPageHead(const T& obj) const {
      for(const uint8_t* page : mPagePool) {
        if(reinterpret_cast<const uint8_t*>(&obj) == page) {
          return true;
        }
      }
      return false;
    }

    //Returns if free list is sorted first by increasing page, then by increasing address within pages
    bool validateFreeIndexes(void) {
      if(mFree.size() <= 1)
        return true;

      //Convenient temporary arrays for debugging
      std::vector<int> testPage;
      std::vector<int> testIndex;
      for(auto it = mFree.begin(); it != mFree.end(); ++it) {
        int page = getPage(*it);
        testPage.push_back(page);
        testIndex.push_back(getIndexInPage(*it, page));
      }

      auto it = mFree.begin();
      T* lastObj = it.dataPointer();
      ++it;
      int lastPage = getPage(*lastObj);

      while(it != mFree.end()) {
        int curPage = getPage(*it);
        T* curObj = it.dataPointer();

        if(lastPage == -1 || curPage == -1 ||
          (curPage == lastPage && lastObj > curObj) ||
          (lastPage > curPage))
          return false;
        ++it;
        lastObj = curObj;
      }
      return true;
    }

    IntrusiveList<T> mObjects;
    IntrusiveList<T> mFree;
    //Vector of pointers to pages, which are buffers of uint8_t in which the Ts are placed
    std::vector<uint8_t*> mPagePool;
    size_t mPageSize = 0;
    size_t mInUse = 0;
#ifdef VECLIST_TRAVERSAL_TRACKING
    unsigned mTraversalSum = 0;
    unsigned mNoOfSamples = 0;
#endif
  };
}