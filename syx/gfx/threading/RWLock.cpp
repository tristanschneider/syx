#include "Precompile.h"
#include "threading/RWLock.h"

void ReaderLock::lock() {
  mRW.readLock();
}

void ReaderLock::unlock() {
  mRW.readUnlock();
}

int RWLock::test() { return 1; }

void WriterLock::lock() {
  mRW.writeLock();
}

void WriterLock::unlock() {
  mRW.writeUnlock();
}