#pragma once

struct TransformEvent {
  TransformEvent() {}
  TransformEvent::TransformEvent(Handle handle, Syx::Mat4 transform)
    : mHandle(handle)
    , mTransform(transform) {
  }

  Handle mHandle;
  Syx::Mat4 mTransform;
};

struct TransformListener {
  void updateLocal() {
    mLocalEvents.clear();
    mMutex.lock();
    mLocalEvents.swap(mEvents);
    mMutex.unlock();
  }

  std::vector<TransformEvent> mEvents;
  //Local buffer used to spend as little time as possible locking event queues
  std::vector<TransformEvent> mLocalEvents;
  std::mutex mMutex;
};