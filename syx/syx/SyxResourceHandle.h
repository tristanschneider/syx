#pragma once

namespace Syx {
  template<class DerivedT, class ExistenceTracker>
  struct EnableDeferredDeletion;

  //Intended as the base class to manage external ownership of a resource through an internal shared pointer
  //The shared pointer is separate from the resource to allow expiration of the handle to act as a destruction request
  //instead of immediately destroying the object. This allows the implementation to decide how and when to appropriately
  //dispose of the internal representation.
  //For opaque handles this can be used as-is, otherwise it can be used as a base class with the rest of the functionality exposed via _get
  template<class Resource, class ExistenceTracker = bool>
  struct DeferredDeleteResourceHandle {
    friend struct EnableDeferredDeletion<Resource, ExistenceTracker>;

    DeferredDeleteResourceHandle()
      : mToken(std::make_shared<ExistenceTracker>()) {
      static_assert(std::is_base_of_v<EnableDeferredDeletion<Resource, ExistenceTracker>, Resource>, "This handle can only be used on types that inherit from EnableDeferredDeletion");
    }

    DeferredDeleteResourceHandle(const DeferredDeleteResourceHandle&) = default;
    DeferredDeleteResourceHandle(DeferredDeleteResourceHandle&&) = default;
    DeferredDeleteResourceHandle& operator=(const DeferredDeleteResourceHandle&) = default;
    DeferredDeleteResourceHandle& operator=(DeferredDeleteResourceHandle&&) = default;

  protected:
    Resource& _get() {
      assert(mResource && "Handle resource should have been assigned upon creation");
      return *mResource;
    }

    const Resource& _get() const {
      assert(mResource && "Handle resource should have been assigned upon creation");
      return *mResource;
    }

  private:
    Resource* mResource = nullptr;
    std::shared_ptr<ExistenceTracker> mToken;
  };

  //Base class for resources that intend to be tracked via DeferredDeleteResourceHandle
  template<class DerivedT, class ExistenceTracker = bool>
  struct EnableDeferredDeletion {
    EnableDeferredDeletion(DeferredDeleteResourceHandle<DerivedT, ExistenceTracker>& owner)
      : mExistenceTracker(owner.mToken) {
      owner.mResource = static_cast<DerivedT*>(this);
    }
    EnableDeferredDeletion(const EnableDeferredDeletion& rhs) {
      _assertTrackerMatches(rhs);
      mExistenceTracker = std::move(rhs.mExistenceTracker);
    }
    EnableDeferredDeletion& operator=(const EnableDeferredDeletion& rhs) {
      _assertTrackerMatches(rhs);
      mExistenceTracker = rhs.mExistenceTracker;
    }
    EnableDeferredDeletion& operator=(EnableDeferredDeletion&& rhs) {
      _assertTrackerMatches(rhs);
      mExistenceTracker = std::move(rhs.mExistenceTracker);
    }

    bool isMarkedForDeletion() const {
      return mExistenceTracker.expired();
    }

    template<class Container>
    static void performDeferredDeletions(Container& container) {
      container.erase(std::partition(container.begin(), container.end(), [](auto& e) { return !e.isMarkedForDeletion(); }), container.end());
    }

  private:
    void _assertTrackerMatches(EnableDeferredDeletion& rhs) {
      assert(mExistenceTracker.lock().get() == rhs.mExistenceTracker.lock().get() && "Ownership of existence tracker should not change");
    }

    std::weak_ptr<ExistenceTracker> mExistenceTracker;
  };
}