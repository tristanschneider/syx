#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>

enum class AsyncStatus {
  Pending,
  Complete,
};

template<class T>
struct AsyncResult {
  struct Void {};
  using TemplateT = T;
  using ValueT = std::conditional_t<std::is_void_v<T>, Void, T>;

  ValueT& operator*() { return mValue; }
  const ValueT& operator*() const { return mValue; }
  ValueT* operator->() { return &mValue; }
  const ValueT* operator->() const { return &mValue; }

  ValueT mValue;
};

template<class ResultT>
struct IAsyncHandle {
  using OnComplete = std::function<void(IAsyncHandle&)>;
  using TemplateT = ResultT;

  virtual ~IAsyncHandle() = default;
  virtual const AsyncResult<ResultT>& getResult() const = 0;
  virtual AsyncResult<ResultT> takeResult() = 0;
  virtual AsyncStatus getStatus() const = 0;
  virtual void then(OnComplete onComplete) = 0;
};

namespace Async {
  template<class T>
  AsyncResult<std::decay_t<T>> createResult(T result) {
    return AsyncResult<std::decay_t<T>>{ std::move(result) };
  }

  inline AsyncResult<void> createResult() {
    return {};
  }

  namespace Details {
    template<class ResultT>
    class CompleteHandle : public IAsyncHandle<ResultT> {
    public:
      CompleteHandle(AsyncResult<ResultT> result)
        : mResult(std::move(result)) {
      }

      const AsyncResult<ResultT>& getResult() const override {
        return mResult;
      }

      AsyncResult<ResultT> takeResult() override {
        return std::move(mResult);
      }

      AsyncStatus getStatus() const override {
        return AsyncStatus::Complete;
      }

      void then(IAsyncHandle<ResultT>::OnComplete onComplete) override {
        onComplete(*this);
      }

    private:
      AsyncResult<ResultT> mResult;
    };

    template<class ResultT>
    class AsyncHandle : public IAsyncHandle<ResultT> {
    public:
      virtual const AsyncResult<ResultT>& getResult() const override {
        assert(mResult && "Task must be complete before accessing result");
        return *mResult;
      }

      virtual AsyncResult<ResultT> takeResult() override {
        assert(mResult && "Task must be complete before accessing result");
        return std::move(*mResult);
      }

      virtual AsyncStatus getStatus() const override {
        return mStatus;
      }

      virtual void then(IAsyncHandle<ResultT>::OnComplete onComplete) override {
        //If task is already done, trigger the callback now
        if(mStatus.load(std::memory_order::memory_order_acquire) == AsyncStatus::Complete) {
          onComplete(*this);
        }
        else {
          //Task is pending, add to list of completion handlers
          std::unique_lock<std::mutex> lock(mMutex);
          //Check again to see if the task completed as the lock was being acquired
          if(mStatus.load(std::memory_order::memory_order_acquire) == AsyncStatus::Complete) {
            //Unlock since now we don't need to put it in the callback container anymore, we can call now, but don't want to do so while holding the lock
            lock.unlock();
            onComplete(*this);
          }
          else {
            //Enqueue to be called later upon completion
            mCompletionHandlers.emplace_back(std::move(onComplete));
          }
        }
      }

      void complete(AsyncResult<ResultT> result) {
        assert(!mResult && "Task should only be completed once");
        mResult = std::move(result);

        //Complete the status first so that callbacks can't delay completion of the task
        mStatus.store(AsyncStatus::Complete, std::memory_order_release);

        //Put the callbacks in a temporary before calling them. This protects against an unnecessary delay for someone who called .then but failed the completion check, meaning they're waiting to add to mCompletionHandlers right now
        std::vector<IAsyncHandle<ResultT>::OnComplete> callbacks;
        {
          std::lock_guard<std::mutex> lock(mMutex);
          callbacks = std::move(mCompletionHandlers);
          mCompletionHandlers.clear();
        }
        //At this point, if someone was waiting on the mutex thinking the task wasn't complete, as soon as they acquire it, they should see that it is, and not add to the completion handlers, instead call the handler immediately

        for(auto&& callback : callbacks) {
          callback(*this);
        }

        assert(mCompletionHandlers.empty() && "No more completion handlers should be added once the task has completed, the handlers should be called directly");
      }

    private:
      using LockGuard = std::lock_guard<std::mutex>;

      std::mutex mMutex;
      std::atomic<AsyncStatus> mStatus = AsyncStatus::Pending;
      std::optional<AsyncResult<ResultT>> mResult;
      std::vector<IAsyncHandle<ResultT>::OnComplete> mCompletionHandlers;
    };
  }

  template<class T>
  std::shared_ptr<Details::AsyncHandle<T>> createAsyncHandle() {
    return std::make_shared<Details::AsyncHandle<T>>();
  }

  template<class T>
  std::shared_ptr<IAsyncHandle<T>> createCompleteHandle(T value) {
    return std::make_shared<Details::CompleteHandle<T>>(Async::createResult(std::move(value)));
  }

  inline std::shared_ptr<IAsyncHandle<void>> createCompleteHandle() {
    return std::make_shared<Details::CompleteHandle<void>>(Async::createResult());
  }

  template<class T>
  void setComplete(Details::AsyncHandle<T>& handle, T result) {
    handle.complete({ std::move(result) });
  }

  inline void setComplete(Details::AsyncHandle<void>& handle) {
    handle.complete({});
  }

  //Create a chained handle using a function that returns an AsyncResult
  template<class Callback, class T>
  auto thenResult(IAsyncHandle<T>& prev, Callback callback) {
    using ResultT = std::invoke_result_t<Callback, IAsyncHandle<T>&>;
    using WrappedType = typename ResultT::TemplateT;
    auto wrapperTask = createAsyncHandle<WrappedType>();
    prev.then([wrapperTask, callback(std::move(callback))](IAsyncHandle<T>& outerTask) {
      wrapperTask->complete(callback(outerTask));
    });
    return wrapperTask;
  }

  //Create a chaned handle using a function that returns a new AsyncHandle
  template<class Callback, class T>
  auto thenHandle(IAsyncHandle<T>& prev, Callback callback) {
    using ResultT = std::invoke_result_t<Callback, IAsyncHandle<T>&>;
    //Dig out the type from the shared pointer to an IAsyncHandle
    using WrappedType = typename ResultT::element_type::TemplateT;
    //Create a wrapper task to represent the final result, the chained task can't start until prev finishes
    auto wrapperTask = createAsyncHandle<WrappedType>();
    prev.then([wrapperTask, callback(std::move(callback))](IAsyncHandle<T>& outerTask) {
      //Prev has finished, use the result to create the task from the callback
      auto chainedTask = callback(outerTask);
      //Forward the result from the chained task to the wrapper task so the caller of thenResult can use it
      chainedTask->then([wrapperTask](auto& original) {
        wrapperTask->complete(original.getResult());
      });
    });
    return wrapperTask;
  }
};