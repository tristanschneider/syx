#pragma once
#include "SyxSmallIndexSet.h"

namespace Syx {
  //Always use this as parameters to give the possibility of trying different volumes
  struct BoundingVolume {
    BoundingVolume(void) {}
    BoundingVolume(const AABB& aabb): mAABB(aabb) {}

    AABB mAABB;
  };

  //Result given from queries
  struct ResultNode {
    ResultNode(void) {}
    ResultNode(Handle handle, void* userdata): mHandle(handle), mUserdata(userdata) {}

    Handle mHandle;
    void* mUserdata;
  };

  struct InsertParam {
    InsertParam(const BoundingVolume& volume, void* userdata)
      : mVolume(volume)
      , mUserdata(userdata) {}

    BoundingVolume mVolume;
    void* mUserdata;
  };

  typedef std::vector<ResultNode> BroadResults;
  typedef std::vector<std::pair<ResultNode, ResultNode>> BroadPairs;

  class BroadphaseContext {
  public:
    virtual ~BroadphaseContext() {}
    BroadResults mQueryResults;
    BroadPairs mQueryPairResults;
  };

  class Broadphase {
  public:
    //Builds a broadphase optimized for being static. Handles provided if a container is given, but likely aren't needed since nodes are static
    virtual void BuildStatic(const std::vector<InsertParam>& nodes, std::vector<Handle>* resultHandles = nullptr) {
      //Up to derived classes to do something clever
      for(const InsertParam& node : nodes) {
        Handle handle = Insert(node.mVolume, node.mUserdata);
        if(resultHandles)
          resultHandles->push_back(handle);
      }
    }

    virtual Handle Insert(const BoundingVolume& obj, void* userdata) = 0;
    virtual void Remove(Handle handle) = 0;
    virtual void Clear(void) = 0;

    virtual void Draw(void) {}
    virtual Handle Update(const BoundingVolume& newVol, Handle handle) = 0;

    virtual void QueryPairs(BroadphaseContext& context) const = 0;
    virtual void QueryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const = 0;
    virtual void QueryVolume(const BoundingVolume& volume, BroadphaseContext& context) const = 0;
    //Maybe also have an interactive query for raycasting one at a time

  protected:
    Handle GetNewHandle(void) { return mHandleGen.Next(); }

  private:
    HandleGenerator mHandleGen;
  };
}