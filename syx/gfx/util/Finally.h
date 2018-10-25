#pragma once

class Finally {
public:
  Finally(std::function<void()> action)
    : mAction(std::move(action)) {
  }

  ~Finally() {
    if(mAction)
      mAction();
  }

  void cancel() {
    mAction = nullptr;
  }

  Finally(const Finally&) = delete;
  Finally(Finally&& rhs) = default;

  Finally& operator=(const Finally&) = delete;
  Finally& operator=(Finally&&) = delete;

private:
  std::function<void()> mAction;
};