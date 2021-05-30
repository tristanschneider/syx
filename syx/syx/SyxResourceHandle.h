#pragma once
#include <optional>

namespace Syx {
  template<class DerivedT>
  struct EnableDeferredDeletion;
  template<class Resource>
  struct WeakDeferredDeleteResourceHandle;

  //Intended as the base class to manage external ownership of a resource through an internal shared pointer
  //The shared pointer is separate from the resource to allow expiration of the handle to act as a destruction request
  //instead of immediately destroying the object. This allows the implementation to decide how and when to appropriately
  //dispose of the internal representation.
  //For opaque handles this can be used as-is, otherwise it can be used as a base class with the rest of the functionality exposed via _get
  template<class Resource>
  struct DeferredDeleteResourceHandle {
    friend struct EnableDeferredDeletion<Resource>;

    DeferredDeleteResourceHandle()
      : mToken(std::make_shared<Resource*>(nullptr)) {
      static_assert(std::is_base_of_v<EnableDeferredDeletion<Resource>, Resource>, "This handle can only be used on types that inherit from EnableDeferredDeletion");
    }

    DeferredDeleteResourceHandle(std::shared_ptr<Resource*> resource)
      : mToken(std::move(resource)) {
    }

    DeferredDeleteResourceHandle(const DeferredDeleteResourceHandle&) = default;
    DeferredDeleteResourceHandle(DeferredDeleteResourceHandle&&) = default;
    DeferredDeleteResourceHandle& operator=(const DeferredDeleteResourceHandle&) = default;
    DeferredDeleteResourceHandle& operator=(DeferredDeleteResourceHandle&&) = default;

    operator WeakDeferredDeleteResourceHandle<Resource>() const {
      return WeakDeferredDeleteResourceHandle<Resource>(mToken);
    }

    operator bool() const {
      return mToken && *mToken;
    }

  protected:
    Resource& _get() {
      assert(mToken && "Handle resource should have been assigned upon creation");
      return **mToken;
    }

    const Resource& _get() const {
      assert(mToken && "Handle resource should have been assigned upon creation");
      return **mToken;
    }

  private:
    std::shared_ptr<Resource*> mToken;
  };

  template<class Resource>
  struct WeakDeferredDeleteResourceHandle {
    WeakDeferredDeleteResourceHandle() = default;
    WeakDeferredDeleteResourceHandle(std::weak_ptr<Resource*> token)
      : mToken(std::move(token)) {
    }
    WeakDeferredDeleteResourceHandle(WeakDeferredDeleteResourceHandle&) = default;
    WeakDeferredDeleteResourceHandle(WeakDeferredDeleteResourceHandle&&) = default;
    WeakDeferredDeleteResourceHandle& operator=(const WeakDeferredDeleteResourceHandle&) = default;
    WeakDeferredDeleteResourceHandle& operator=(WeakDeferredDeleteResourceHandle&&) = default;

    DeferredDeleteResourceHandle<Resource> lock() const {
      return DeferredDeleteResourceHandle<Resource>(mToken.lock());
    }

  private:
    std::weak_ptr<Resource*> mToken;
  };

  //Base class for resources that intend to be tracked via DeferredDeleteResourceHandle
  template<class DerivedT>
  struct EnableDeferredDeletion {
    EnableDeferredDeletion(DeferredDeleteResourceHandle<DerivedT>& owner)
      : mExistenceTracker(owner.mToken) {
      *owner.mToken = static_cast<DerivedT*>(this);
    }
    EnableDeferredDeletion(EnableDeferredDeletion&& rhs) {
      mExistenceTracker = std::move(rhs.mExistenceTracker);
      _reassignTracker();
    }
    EnableDeferredDeletion& operator=(EnableDeferredDeletion&& rhs) {
      mExistenceTracker = std::move(rhs.mExistenceTracker);
      _reassignTracker();
      return *this;
    }
    ~EnableDeferredDeletion() {
      _releaseTracker();
    }

    bool isMarkedForDeletion() const {
      return mExistenceTracker.expired();
    }

    std::optional<DeferredDeleteResourceHandle<DerivedT>> duplicateHandle() {
      if(auto token = mExistenceTracker.lock()) {
        auto result = std::make_optional(DeferredDeleteResourceHandle<DerivedT>());
        result->mToken = token;
        return result;
      }
      return std::nullopt;
    }

    void* _toUserdata() {
      auto strong = mExistenceTracker.lock();
      //Encode the stable pointer to the token in void*. This is not the derived pointer, but the pointer to that pointer
      //which will be consistent even if the handle moves. This is still unsafe to retain beyond the lifetime of the handle
      //because it can't be checked
      return strong ? static_cast<void*>(&*strong) : nullptr;
    }

    //Must only be used if caller can guarantee the userdata won't outlive the handle
    static DerivedT* _fromUserdata(void* userdata) {
      return *reinterpret_cast<DerivedT**>(userdata);
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
    void _reassignTracker() {
      if(std::shared_ptr<DerivedT*> self = mExistenceTracker.lock()) {
        *self = static_cast<DerivedT*>(this);
      }
    }

    void _releaseTracker() {
      if(std::shared_ptr<DerivedT*> self = mExistenceTracker.lock()) {
        *self = nullptr;
      }
    }

    std::weak_ptr<DerivedT*> mExistenceTracker;
  };
}