#pragma once
//thread_local can be used for static variables, but this is needed to have thread local members

template <typename T>
class ThreadLocal {
public:
  using Constructor = std::function<std::unique_ptr<T>()>;

  //Function that creates the nested object, make_unique with no args is used if none is provided
  ThreadLocal(Constructor c = nullptr)
    : mConstructor(c) {
    if(!mConstructor) {
      mConstructor = []() {
        return std::make_unique<T>();
      };
    }
  }

  T& get() {
    mLock.readLock();
    const std::thread::id& id = std::this_thread::get_id();
    auto it = mPool.find(id);
    //If object already exists in pool, return that
    if(it != mPool.end()) {
      T& result = *it->second;
      mLock.readUnlock();
      return result;
    }

    //Object doesn't exist in pool, create it, put it in the pool, and return it
    mLock.readUnlock();
    std::unique_ptr<T> newObj = mConstructor();
    T& result = *newObj;

    auto lock = mLock.getWriter();
    mPool[id] = std::move(newObj);
    return result;
  }

private:
  Constructor mConstructor;
  std::unordered_map<std::thread::id, std::unique_ptr<T>> mPool;
  RWLock mLock;
};