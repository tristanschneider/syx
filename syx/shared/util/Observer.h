#pragma once
template<typename T>
class Observer;

#pragma warning (disable: 4521)

template<typename Obs, typename Inner>
class Subject {
  friend class Observer<Inner>;

public:
  ~Subject() {
    for(Obs* o : mObservers)
      o->mSubject = nullptr;
    mObservers.clear();
  }

  const std::vector<Obs*> get() const {
    return mObservers;
  }

private:
  //Called through observer's observe()
  void add(Obs& observer) {
    mObservers.push_back(&observer);
  }

  void remove(Obs& observer) {
    for(size_t i = 0; i < mObservers.size(); ++i) {
      if(mObservers[i] == &observer) {
        observer.mSubject = nullptr;
        mObservers[i] = mObservers.back();
        mObservers.pop_back();
        return;
      }
    }
    assert(false); //Should have found observer in the list
  }

  std::vector<Obs*> mObservers;
};

template<typename T>
class Observer {
public:
  using SubjectType = Subject<Observer<T>, T>;
  friend class SubjectType;

  template<typename... Args>
  Observer(Args&&... args)
    : mObj(std::forward<Args>(args)...)
    , mSubject(nullptr) {
  }

  Observer(const Observer<T>& observer)
    : mObj(observer.mObj)
    , mSubject(nullptr) {
    observe(observer.mSubject);
  }

  //Needed to prefer copy constructor over variatic template constructor
  Observer(Observer<T>& observer)
    : Observer(const_cast<const Observer<T>&>(observer)) {
  }

  Observer(Observer<T>&& observer)
    : mObj(std::move(observer.mObj)) {
    if(observer.mSubject) {
      observer.mSubject->remove(observer);
      observer.mSubject->add(*this);
    }
  }

  Observer<T>& operator=(const Observer<T>& observer) {
    if(this == &observer)
      return *this;
    if(mSubject)
      mSubject->remove(*this);

    mObj = observer.mObj;
    if(observer.mSubject)
      observer.mSubject->add(*this);
    return *this;
  }

  ~Observer() {
    if(mSubject)
      mSubject->remove(*this);
  }

  T& get() {
    return mObj;
  }

  const T& get() const {
    return mObj;
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
  T mObj;
};

#pragma warning (default: 4521)
