#pragma once

class Variant {
public:
  enum class Type : uint8_t {
    None,
    Int,
    Float,
    String,
    VoidPtr,
  };

  Variant()
    : mPtrData(nullptr)
    , mType(Type::None) {
  }

  template<class T>
  Variant(T&& value)
    : Variant() {
    set(std::forward<T>(value));
  }

  Variant(const Variant&) = default;
  Variant(Variant&&) = default;
  Variant& operator=(const Variant&) = default;
  Variant& operator=(Variant&&) = default;

  Type getType() const {
    return mType;
  }

  template<class T, typename std::enable_if_t<std::is_integral_v<T>>* = nullptr>
  T get() const {
    return static_cast<T>(mIntData);
  }

  template<class T, typename std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
  T get() const {
    return static_cast<T>(mFloatData);
  }

  template<class T, typename std::enable_if_t<std::is_same_v<T, std::string>>* = nullptr>
  const T& get() const {
    return mStrData;
  }

  template<class T, typename std::enable_if_t<std::is_pointer_v<T>>* = nullptr>
  T get() {
    return static_cast<T>(mPtrData);
  }

  void set(std::string data) {
    mType = Type::String;
    mStrData = std::move(data);
  }

  void set(const char* data) {
    mType = Type::String;
    mStrData = data;
  }

  template<class T, typename std::enable_if_t<std::is_integral_v<T>>* = nullptr>
  void set(T data) {
    mType = Type::Int;
    mIntData = static_cast<uint64_t>(data);
  }

  template<class T, typename std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
  void set(T data) {
    mType = Type::Float;
    mFloatData = static_cast<double>(data);
  }

  template<class T, typename std::enable_if_t<std::is_pointer_v<T>>* = nullptr>
  void set(T data) {
    mType = Type::VoidPtr;
    mPtrData = static_cast<void*>(data);
  }

private:
  union {
    uint64_t mIntData;
    double mFloatData;
    void* mPtrData;
  };
  std::string mStrData;
  Type mType;
};