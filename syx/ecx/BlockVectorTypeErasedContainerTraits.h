#pragma once

#include "BlockVector.h"
#include "TypeErasedContainer.h"

namespace ecx {
  template<class Allocator = std::allocator<uint8_t>>
  struct BlockVectorTypeErasedContainerTraits : TypeErasedContainer::ITraits {
    BlockVectorTypeErasedContainerTraits(const IRuntimeTraits& traits)
      : mTraits(&traits) {
    }

    //Function that can be used to create storage through LinearEntityRegistry::addRuntimeComponent
    //This is the function pointer, pass through BlockVectorTypeErasedContainerTraits as the data
    static TypeErasedContainer createStorage(void* self) {
      return TypeErasedContainer(*static_cast<TypeErasedContainer::ITraits*>(self));
    }

    typeId_t<TypeErasedTag> type() const override {
      //TODO: what should this be?
      return {};
    }

    void* create() const override {
      return new BlockVector<Allocator>(*mTraits);
    }

    void destroy(void* data) const override {
      delete _cast(data);
    }

    void swap(void* data, size_t indexA, size_t indexB) const override {
      BlockVector<Allocator>* container = _cast(data);
      //Hack to use the end of the container as a temporary
      void* temp = container->emplace_back();
      void* a = container->at(indexA);
      void* b = container->at(indexB);
      mTraits->moveAssign(a, temp);
      mTraits->moveAssign(b, a);
      mTraits->moveAssign(temp, a);
      container->pop_back();
    }

    void pop_back(void* data) const override {
      _cast(data)->pop_back();
    }

    size_t size(const void* data) const override {
      return _cast(data)->size();
    }

    void moveIntoFromIndex(size_t index, void* from, void* to) const override {
      BlockVector<Allocator>* f = _cast(from);
      BlockVector<Allocator>* t = _cast(to);
      assert(mTraits == f->getTraits() && mTraits == t->getTraits());
      void* result = t->emplace_back();
      mTraits->moveAssign(f->at(index), result);
    }

    void push_back(void* data) const override {
      _cast(data)->emplace_back();
    }

    void clear(void* data) const override {
      _cast(data)->clear();
    }

    void* at(void* data, size_t index) const override {
      return _cast(data)->at(index);
    }

    void resize(void* data, size_t size) const override {
      _cast(data)->resize(size);
    }

    static BlockVector<Allocator>* _cast(void* data) {
      return static_cast<BlockVector<Allocator>*>(data);
    }

    static const BlockVector<Allocator>* _cast(const void* data) {
      return static_cast<const BlockVector<Allocator>*>(data);
    }

    const IRuntimeTraits* mTraits = nullptr;
  };
}