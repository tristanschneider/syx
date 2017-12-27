#include "Precompile.h"
#include "threading/RWLock.h"

void ReaderLock::lock() {
  mRW.readLock();
}

bool ReaderLock::try_lock() {
  return mRW.tryReadLock();
}

void ReaderLock::unlock() {
  mRW.readUnlock();
}

void WriterLock::lock() {
  mRW.writeLock();
}

bool WriterLock::try_lock() {
  return mRW.tryWriteLock();
}

void WriterLock::unlock() {
  mRW.writeUnlock();
}