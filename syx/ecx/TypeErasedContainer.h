#pragma once

struct TypeErasedTag {};

// Own a container but without requiring a template argument to depend on the element type
template<template<class> class ContainerT>
struct TypeErasedContainer {
  template<class T>
  static TypeErasedContainer create() {
    return TypeErasedContainer(new ContainerT<T>(),
      typeId<T, TypeErasedTag>(),
      [](void* data) { delete static_cast<ContainerT<T>*>(data); }
    );
  }

  //Needs to be public to allow make_unique, but intended to be used only through create<T>()
  TypeErasedContainer(void* data, typeId_t<TypeErasedTag> type, std::function<void(void*)> destructor)
    : mData(data)
    , mType(type)
    , mDestructor(std::move(destructor)) {
  }

  TypeErasedContainer(TypeErasedContainer&& rhs)
    : mData(rhs.mData)
    , mType(rhs.mType)
    , mDestructor(std::move(rhs.mDestructor)) {
    rhs.mDestructor = nullptr;
    rhs.mData = nullptr;
  }

  TypeErasedContainer& operator=(TypeErasedContainer&& rhs) {
    if(this != &rhs) {
      _reset();
      mData = rhs.mData;
      mType = rhs.mType;
      mDestructor = std::move(rhs.mDestructor);

      rhs.mData = nullptr;
      rhs.mDestructor = nullptr;
    }
    return *this;
  }

  TypeErasedContainer(const TypeErasedContainer&) = delete;
  TypeErasedContainer& operator=(const TypeErasedContainer&) = delete;

  ~TypeErasedContainer() {
    _reset();
  }

  template<class T>
  ContainerT<T>* get() {
    return _assertCorrectType<T>() ? static_cast<ContainerT<T>*>(mData) : nullptr;
  }

  template<class T>
  const ContainerT<T>* get() const {
    return _assertCorrectType<T>() ? static_cast<const ContainerT<T>*>(mData) : nullptr;
  }

private:
  void _reset() {
    if(mDestructor) {
      mDestructor(mData);
      mDestructor = nullptr;
      mData = nullptr;
    }
  }

  template<class T>
  bool _assertCorrectType() const {
    const bool correctType = mType == typeId<T, TypeErasedTag>();
    assert(correctType && "ComponentContainer should always be used with the same type");
    return correctType;
  }

  void* mData = nullptr;
  typeId_t<TypeErasedTag> mType;
  std::function<void(void*)> mDestructor;
};