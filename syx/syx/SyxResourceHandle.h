#pragma once
#include <optional>

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
      return *this;
    }
    EnableDeferredDeletion& operator=(EnableDeferredDeletion&& rhs) {
      _assertTrackerMatches(rhs);
      mExistenceTracker = std::move(rhs.mExistenceTracker);
      return *this;
    }

    bool isMarkedForDeletion() const {
      return mExistenceTracker.expired();
    }

    std::optional<DeferredDeleteResourceHandle<DerivedT, ExistenceTracker>> duplicateHandle() {
      if(auto token = mExistenceTracker.lock()) {
        auto result = std::make_optional(DeferredDeleteResourceHandle<DerivedT, ExistenceTracker>());
        result->mResource = static_cast<DerivedT*>(this);
        result->mToken = token;
        return result;
      }
      return std::nullopt;
    }

    //Container of EnableDeferredDeletions
    template<class ValueT, class Container
      , std::enable_if_t<std::is_same_v<ValueT, DerivedT>, bool> = true>
    static void _performDeferredDeletions(Container& container) {
      container.erase(std::partition(container.begin(), container.end(), [](auto& e) { return !e.isMarkedForDeletion(); }), container.end());
    }

    //Container of pointers to EnableDeferredDeletions
    template<class ValueT, class Container
      , std::enable_if_t<std::is_pointer_v<ValueT> && std::is_same_v<std::remove_pointer_t<ValueT>, DerivedT>, bool> = true>
    static void _performDeferredDeletions(Container& container) {
      container.erase(std::partition(container.begin(), container.end(), [](auto& e) { return !e->isMarkedForDeletion(); }), container.end());
    }

    //Container of smart pointers to EnableDeferredDeletions
    template<class ValueT, class Container
      , std::enable_if_t<std::is_same_v<typename ValueT::element_type, DerivedT>, bool> = true>
    static void _performDeferredDeletions(Container& container) {
      container.erase(std::partition(container.begin(), container.end(), [](auto& e) { return !e->isMarkedForDeletion(); }), container.end());
    }

    //Map of something to EnableDeferredDeletions
    template<class ValueT, class Container
      , std::enable_if_t<std::is_same_v<typename ValueT::second_type, DerivedT>, bool> = true>
    static void _performDeferredDeletions(Container& c) {
      for(auto it = c.begin(); it != c.end();) {
        if(it->second.isMarkedForDeletion()) {
          it = c.erase(it);
        }
        else {
          ++it;
        }
      }
    }

    //Map of something to smart pointers to EnableDeferredDeletions
    template<class ValueT, class Container
      , std::enable_if_t<std::is_same_v<typename ValueT::second_type::element_type, DerivedT>, bool> = true>
    static void _performDeferredDeletions(Container& c) {
      for(auto it = c.begin(); it != c.end();) {
        if(it->second->isMarkedForDeletion()) {
          it = c.erase(it);
        }
        else {
          ++it;
        }
      }
    }


    template<class Container>
    static void performDeferredDeletions(Container& container) {
      _performDeferredDeletions<Container::value_type>(container);
    }

  private:
    void _assertTrackerMatches(const EnableDeferredDeletion& rhs) {
      assert(mExistenceTracker.lock().get() == rhs.mExistenceTracker.lock().get() && "Ownership of existence tracker should not change");
    }

    std::weak_ptr<ExistenceTracker> mExistenceTracker;
  };
}