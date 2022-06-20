#pragma once

namespace ecx {
  class AnyType {
  public:
    struct ITraits {
      virtual ~ITraits() = default;
      virtual void* create() const = 0;
      virtual void* createInPlace(void* dst, const void* src) const = 0;
      virtual void destroy(void*) const = 0;
      virtual void* copyConstruct(const void* src) const = 0;
      virtual void copyAssign(void* dst, const void* src) const = 0;
      virtual bool areEqual(const void* a, const void* b) const = 0;
      virtual size_t size() const = 0;
    };

    //Deduce if operator== exists, otherwise return false
    template<class T, class Enabled = void>
    struct MaybeEqual {
      static bool areEqual(const T&, const T&) {
        return false;
      }
    };
    template<class T>
    struct MaybeEqual<T, std::enable_if_t<std::is_same_v<bool, decltype(std::declval<T>() == std::declval<T>())>>> {
      static bool areEqual(const T& l, const T& r) {
        return l == r;
      }
    };

    template<class T>
    struct Traits : ITraits {
      static constexpr Traits& singleton() {
        static Traits<T> result;
        return result;
      }

      void* create() const override {
        return new T();
      }

      void* createInPlace(void* dst, const void* src) const override {
        if constexpr(std::is_copy_assignable_v<T>) {
          return new (dst) T (_cast(src));
        }
        else {
          assert(false && "Type must be copy constructible");
          return new (dst) T();
        }
      }

      //Caller responsible for delete, this is only supposed to call destructor in case memory should be re-used
      void destroy(void* p) const override {
        _cast(p).~T();
      }

      void* copyConstruct(const void* src) const override {
        if constexpr(std::is_copy_constructible_v<T>) {
          return new T(_cast(src));
        }
        else {
          assert(false && "Type must be copy constructible");
          return new T();
        }
      }

      void copyAssign(void* dst, const void* src) const override {
        if constexpr(std::is_copy_assignable_v<T>) {
          _cast(dst) = _cast(src);
        }
        else {
          assert(false && "Type must be assignable");
        }
      }

      bool areEqual(const void* a, const void* b) const override {
        return MaybeEqual<T>::areEqual(_cast(a), _cast(b));
      }

      size_t size() const override {
        return sizeof(T);
      }

    private:
      static T& _cast(void* p) {
        return *static_cast<T*>(p);
      }

      static const T& _cast(const void* p) {
        return *static_cast<const T*>(p);
      }

      Traits() = default;
    };

    AnyType() = default;

    ~AnyType() {
      if(mTraits) {
        mTraits->destroy(mData);
        delete mData;
      }
    }

    AnyType(const AnyType& other) noexcept
      : mTraits(other.mTraits)
      , mData(mTraits ? mTraits->copyConstruct(other.mData) : nullptr) {
    }

    AnyType(AnyType&& other) noexcept
      : mTraits(other.mTraits)
      , mData(other.mData) {
      other._release();
    }

    AnyType& operator=(const AnyType& other) {
      //Self assignment
      if(mData == other.mData) {
        return *this;
      }

      //Memory can be re-used if new object is the same size or smaller
      if(mTraits && other.mTraits && mTraits->size() >= other.mTraits->size()) {
        //Call destructor without freeing memory
        mTraits->destroy(mData);
        mData = other.mTraits->createInPlace(mData, other.mData);
        mTraits = other.mTraits;
        return *this;
      }

      //If this had a value, free it first
      _delete();

      mTraits = other.mTraits;
      //If the new type is non-empty, copy construct the new value
      if (mTraits) {
        mData = mTraits->copyConstruct(other.mData);
      }

      return *this;
    }

    AnyType& operator=(AnyType&& other) {
      if(mData == other.mData) {
        return *this;
      }

      _delete();

      mTraits = other.mTraits;
      mData = other.mData;

      other._release();

      return *this;
    }

    bool empty() const {
      return mTraits == nullptr;
    }

    operator bool() const {
      return !empty();
    }

    bool operator==(const AnyType& rhs) const {
      if(mTraits == rhs.mTraits) {
        //If traits exist, check equality, if it doesn't, both are empty
        return mTraits ? mTraits->areEqual(mData, rhs.mData) : true;
      }
      return false;
    }

    bool operator!=(const AnyType& rhs) const {
      return !(*this == rhs);
    }

    template<class T>
    T& get() {
      return *reinterpret_cast<T*>(mData);
    }

    template<class T>
    const T& get() const {
      return *reinterpret_cast<const T*>(mData);
    }

    template<class T>
    T* tryGet() {
      if(mTraits == &Traits<std::decay_t<T>>::singleton()) {
        return &get<T>();
      }
      return nullptr;
    }

    template<class T>
    const T* tryGet() const {
      if(mTraits == &Traits<std::decay_t<T>>::singleton()) {
        return &get<T>();
      }
      return nullptr;
    }

    template<class T, class... Args>
    AnyType& emplace(Args&&... args) {
      const auto& traits = Traits<T>::singleton();
      //Re-use existing memory if it's big enough
      if(mTraits && mTraits->size() >= traits.size()) {
        mTraits->destroy(mData);
        //Launder is in general needed when re-using object storage. Not sure about this particular case, but can't hurt
        mData = std::launder(new (mData) T{std::forward<Args>(args)...});
        mTraits = &traits;
        return *this;
      }

      mData = new T{std::forward<Args>(args)...};
      mTraits = &traits;
      return *this;
    }

    template<class T, class... Args>
    static AnyType create(Args&&... args) {
      AnyType result;
      result.emplace<T>(std::forward<Args>(args)...);
      return result;
    }

  private:
    //Reset values without deallocating
    void _release() {
      mTraits = nullptr;
      mData = nullptr;
    }

    void _delete() {
      if(mTraits) {
        mTraits->destroy(mData);
        delete mData;
        mData = nullptr;
      }
      mTraits = nullptr;
    }

    //The assumption is either neither exist (empty) or both do
    const ITraits* mTraits = nullptr;
    void* mData = nullptr;
  };
}