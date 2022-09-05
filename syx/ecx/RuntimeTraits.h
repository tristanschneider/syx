#pragma once

namespace ecx {
  struct IRuntimeTraits {
    virtual ~IRuntimeTraits() = default;
    virtual size_t size() const = 0;
    virtual void destruct(void* target) const = 0;
    virtual void defaultConstruct(void* target) const = 0;
    virtual void moveConstruct(void* from, void* to) const = 0;
    virtual void copyConstruct(const void* from, void* to) const = 0;
    virtual void moveAssign(void* from, void* to) const = 0;
    virtual void copyAssign(void* from, void* to) const = 0;
  };

  struct CompositeRuntimeTraits : public IRuntimeTraits {
    CompositeRuntimeTraits(const std::vector<const IRuntimeTraits*>& traits)
      : mTraits(traits.size()) {
      for(size_t i = 0; i < traits.size(); ++i) {
        //TODO: padding for alignment?
        mTraits[i].mOffset = mTotalSize;
        mTraits[i].mTraits = traits[i];
        mTotalSize += traits[i]->size();
      }
    }

    size_t size() const override {
      return mTotalSize;
    }

    void destruct(void* target) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->destruct(_offset(target, t.mOffset));
      }
    }

    void defaultConstruct(void* target) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->defaultConstruct(_offset(target, t.mOffset));
      }
    }

    void moveConstruct(void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->moveConstruct(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    void copyConstruct(const void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->copyConstruct(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    void moveAssign(void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->moveAssign(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    void copyAssign(void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->copyAssign(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    static void* _offset(void* ptr, size_t o) {
      return static_cast<uint8_t*>(ptr) + o;
    }

    static const void* _offset(const void* ptr, size_t o) {
      return static_cast<const uint8_t*>(ptr) + o;
    }

    struct Traits {
      size_t mOffset = 0;
      const IRuntimeTraits* mTraits = nullptr;
    };
    std::vector<Traits> mTraits;
    size_t mTotalSize = 0;
  };

  struct MemCopyRuntimeTraits : IRuntimeTraits {
    MemCopyRuntimeTraits(size_t s)
      : mSize(s) {
    }

    size_t size() const override {
      return mSize;
    }

    void destruct(void*) const override {}

    void defaultConstruct(void*) const override {}

    void moveConstruct(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    void copyConstruct(const void* from, void* to) const override {
      std::memcpy(to, from, mSize);
    }

    void moveAssign(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    void copyAssign(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    const size_t mSize = 0;
  };

  template<class T>
  struct BasicRuntimeTraits : public IRuntimeTraits {
    static const IRuntimeTraits& singleton() {
      static BasicRuntimeTraits<T> result;
      return result;
    }

    size_t size() const override {
      return sizeof(T);
    }

    void destruct(void* target) const override {
      static_cast<T*>(target)->~T();
    }

    void defaultConstruct(void* target) const override {
      new (target) T();
    }

    void moveConstruct(void* from, void* to) const override {
      new (to) T(std::move(*static_cast<T*>(from)));
    }

    void copyConstruct(const void* from, void* to) const override {
      if constexpr(std::is_copy_constructible_v<T>) {
        new (to) T(*static_cast<const T*>(from));
      }
      else {
        assert(false && "Type must be copy constructable or call must be avoided");
      }
    }

    void moveAssign(void* from, void* to) const override {
      *static_cast<T*>(to) = std::move(*static_cast<T*>(from));
    }

    void copyAssign(void* from, void* to) const override {
      if constexpr(std::is_copy_assignable_v<T>) {
        *static_cast<T*>(to) = *static_cast<T*>(from);
      }
      else {
        assert(false && "Type must be copy assignable or call must be avoided");
      }
    }
  };
}