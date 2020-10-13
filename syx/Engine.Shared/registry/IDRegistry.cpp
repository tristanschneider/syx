#include "Precompile.h"
#include "registry/IDRegistry.h"

#include <optional>

namespace {
  struct ClaimedUniqueID : public IClaimedUniqueID {
    ClaimedUniqueID(UniqueID id, std::function<void(const UniqueID&)> onDestruction)
      : mID(id)
      , mOnDestruction(std::move(onDestruction)) {
    }

    ~ClaimedUniqueID() {
      if(mOnDestruction) {
        mOnDestruction(mID);
        mOnDestruction = nullptr;
      }
    }

    ClaimedUniqueID(const ClaimedUniqueID&) = delete;
    ClaimedUniqueID(ClaimedUniqueID&&) = delete;
    ClaimedUniqueID& operator=(const ClaimedUniqueID&) = delete;
    ClaimedUniqueID& operator=(ClaimedUniqueID&&) = delete;

    const UniqueID& operator*() const override {
      return mID;
    }

    UniqueID mID;
    std::function<void(const UniqueID&)> mOnDestruction;
  };

  struct IDRegistry : public IIDRegistry {
  public:
    using LockGuard = std::lock_guard<std::mutex>;

    IDRegistry()
      : mState(std::make_shared<State>()) {
    }

    std::unique_ptr<IClaimedUniqueID> generateNewUniqueID() override {
      return createClaimedID([this] {
        LockGuard lock(mState->mMutex);
        //Keep generating random ids until one works
        UniqueID newId;
        do {
          newId = UniqueID::random();
        }
        while(!mState->mIDs.insert(newId).second);
        return newId;
      }());
    }

    std::unique_ptr<IClaimedUniqueID> tryClaimKnownID(const UniqueID& id) override {
      auto tryClaim = [this, &id] {
        LockGuard lock(mState->mMutex);
        return mState->mIDs.insert(id).second;
      };

      return tryClaim() ? createClaimedID(id) : nullptr;
    }

  private:
    //Capture state ins a shared pointer to allow threadsafe capturing regardless of lifetime
    struct State {
      std::unordered_set<UniqueID> mIDs;
      std::mutex mMutex;
    };

    std::unique_ptr<IClaimedUniqueID> createClaimedID(const UniqueID& id) {
      return std::make_unique<ClaimedUniqueID>(id, [state(std::weak_ptr<State>(mState))](const UniqueID toDestroy) {
        if(auto strongState = state.lock()) {
          LockGuard lock(strongState->mMutex);
          if(auto it = strongState->mIDs.find(toDestroy); it != strongState->mIDs.end()) {
            strongState->mIDs.erase(it);
          }
          else {
            assert(false && "Claimed id should be in map or lifetime tracking has failed");
          }
        }
      });
    }

    std::shared_ptr<State> mState;
  };
}

namespace create {
  std::unique_ptr<IIDRegistry> idRegistry() {
    return std::make_unique<IDRegistry>();
  }
}