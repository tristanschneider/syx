#pragma once

class RWLock;

//Wrappers to allow std::unique_lock use
class ReaderLock {
public:
  ReaderLock(RWLock& rw) : mRW(rw) {}
  void lock();
  void unlock();
  RWLock& mRW;
};

class WriterLock {
public:
  WriterLock(RWLock& rw) : mRW(rw) {}
  void lock();
  void unlock();
  RWLock& mRW;
};

class RWLock {
public:
  RWLock()
    : mReaders(0)
    , mRL(*this)
    , mWL(*this) {
  }

  int test();

  std::unique_lock<ReaderLock> getReader() {
    return std::unique_lock<ReaderLock>(mRL);
  }

  std::unique_lock<WriterLock> getWriter() {
    return std::unique_lock<WriterLock>(mWL);
  }

  void readLock() {
    int readers = mReaders;
    //If negative, wait until readers is positive, then increment
    do {
      readers = std::max(0, readers);
    }
    while(!mReaders.compare_exchange_weak(readers, readers + 1));
  }

  void writeLock() {
    //Keep trying until we can set from 0 to -1, meaning there are no readers and we indicated write status
    int noReaders = 0;
    while(!mReaders.compare_exchange_weak(noReaders, -1)) {
      noReaders = 0;
    }
  }

  void readUnlock() {
    mReaders.fetch_sub(1);
  }

  void writeUnlock() {
    mReaders.fetch_add(1);
  }

private:
  //Number of locked readers. -1 means writing
  std::atomic_int mReaders;
  //Need persistent storage as 
  ReaderLock mRL;
  WriterLock mWL;
};
