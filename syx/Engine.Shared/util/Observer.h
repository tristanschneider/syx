#pragma once
template<class T, class Mutex>
class Observer;

#pragma warning (disable: 4521)

//TODO: make a template for this
//Call method on all observers of subject. Not threadsafe.
#define CallOnObserversPtr(subject, method, ...) { for(auto o : subject.get()) o->method(__VA_ARGS__); }
#define CallOnObservers(subject, method, ...) { for(auto o : subject.get()) o.method(__VA_ARGS__); }

struct NoMutex {
  void lock() {}
  void unlock() {}
};

template<class Obs, class Friend, class Mutex>
class Subject {
  friend Friend;

public:
  ~Subject() {
    std::lock_guard<Mutex> lock(mMutex);
    for(Obs* o : mObservers)
      o->mSubject = nullptr;
    mObservers.clear();
  }

  const std::vector<Obs*>& get() const {
    return mObservers;
  }

  template<class Func>
  void forEach(const Func& func) {
    std::lock_guard<Mutex> lock(mMutex);
    for(Obs* o : mObservers)
      func(*o);
  }

private:
  //Called through observer's observe()
  void add(Friend& observer) {
    std::lock_guard<Mutex> lock(mMutex);
    mObservers.push_back(reinterpret_cast<Obs*>(&observer));
  }

  void remove(Friend& observer) {
    std::lock_guard<Mutex> lock(mMutex);
    for(size_t i = 0; i < mObservers.size(); ++i) {
      if(mObservers[i] == reinterpret_cast<Obs*>(&observer)) {
        observer.mSubject = nullptr;
        mObservers[i] = mObservers.back();
        mObservers.pop_back();
        return;
      }
    }
    assert(false); //Should have found observer in the list
  }

  std::vector<Obs*> mObservers;
  Mutex mMutex;
};

template<class T, class Mutex = NoMutex>
class Observer {
public:
  using SubjectType = Subject<T, Observer<T>, Mutex>;
  friend class SubjectType;

  Observer()
    : mSubject(nullptr) {
  }

  Observer(const Observer&) = delete;

  Observer(Observer&&) = delete;

  Observer<T>& operator=(const Observer&) = delete;

  Observer& operator=(Observer&&) = delete;

  virtual ~Observer() {
    if(mSubject)
      mSubject->remove(*this);
  }

  bool hasSubject() const {
    return mSubject != nullptr;
  }

  //Observe the given subject. Multiple subjects cannot be observed by a single observer
  //Null can be provided to remove any current subject
  void observe(SubjectType* subject) {
    if(mSubject == subject)
      return;
    //Remove any previous subject
    if(mSubject)
      mSubject->remove(*this);
    //Add new subject
    if(subject)
      subject->add(*this);
    mSubject = subject;
  }

private:
  //Set by subject on add/remove
  SubjectType* mSubject;
};

#pragma warning (default: 4521)
