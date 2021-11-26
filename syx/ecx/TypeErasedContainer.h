#pragma once

struct TypeErasedTag {};

// Own a container but without requiring a template argument to depend on the element type
template<template<class> class ContainerT>
struct TypeErasedContainer {
  struct ITraits {
    virtual ~ITraits() = default;
    virtual typeId_t<TypeErasedTag> type() const = 0;
    virtual void* create() const = 0;
    virtual void destroy(void* data) const = 0;
    virtual void swap(void* data, size_t indexA, size_t indexB) const = 0;
    virtual void pop_back(void* data) const = 0;
    virtual size_t size(const void* data) const = 0;
  };

  template<class T>
  struct Traits : public ITraits {
    static const Traits& singleton() {
      static Traits result;
      return result;
    }

    typeId_t<TypeErasedTag> type() const {
      return typeId<T, TypeErasedTag>();
    }

    void* create() const override {
      return new ContainerT<T>();
    }

    void destroy(void* data) const override {
      delete &_cast(data);
    }

    void swap(void* data, size_t indexA, size_t indexB) const override {
      std::swap(_cast(data)[indexA], _cast(data)[indexB]);
    }

    void pop_back(void* data) const override {
      _cast(data).pop_back();
    }

    virtual size_t size(const void* data) const override {
      return _cast(data).size();
    }

    ContainerT<T>& _cast(void* data) const {
      return *static_cast<ContainerT<T>*>(data);
    }

    const ContainerT<T>& _cast(const void* data) const {
      return *static_cast<const ContainerT<T>*>(data);
    }

  private:
    //This is intended to only be used through the singleton so the container doesn't
    //have to worry about the traits pointer going bad
    Traits() = default;
    Traits(const Traits&) = delete;
  };

  template<class T>
  static TypeErasedContainer create() {
    return TypeErasedContainer(Traits<T>::singleton());
  }

  //Needs to be public to allow make_unique, but intended to be used only through create<T>()
  TypeErasedContainer(const ITraits& traits)
    : mData(traits.create())
    , mTraits(&traits) {
  }

  TypeErasedContainer(TypeErasedContainer&& rhs)
    : mData(rhs.mData)
    , mTraits(rhs.mTraits) {
    rhs.mTraits = nullptr;
    rhs.mData = nullptr;
  }

  TypeErasedContainer& operator=(TypeErasedContainer&& rhs) {
    if(this != &rhs) {
      _reset();
      mData = rhs.mData;
      mTraits = rhs.mTraits;

      rhs.mData = nullptr;
      rhs.mTraits = nullptr;
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

  void swap(size_t indexA, size_t indexB) {
    mTraits->swap(mData, indexA, indexB);
  }

  void pop_back() {
    mTraits->pop_back(mData);
  }

  size_t size() const {
    return mTraits->size(mData);
  }

private:
  void _reset() {
    if(mTraits) {
      mTraits->destroy(mData);
      mTraits = nullptr;
      mData = nullptr;
    }
  }

  template<class T>
  bool _assertCorrectType() const {
    const bool correctType = mTraits->type() == typeId<T, TypeErasedTag>();
    assert(correctType && "ComponentContainer should always be used with the same type");
    return correctType;
  }

  void* mData = nullptr;
  const ITraits* mTraits = nullptr;
};