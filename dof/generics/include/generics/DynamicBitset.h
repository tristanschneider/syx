#pragma once

namespace gnx {
  namespace bitops {
    constexpr size_t bitsToBytes(size_t bits) {
      return bits/8;
    }

    constexpr size_t bytesToContainBits(size_t bits) {
      return bits % 8 ? bits / 8 + 1 : bits / 8;
    }

    //Set the lower X amount of bits: calling this with 2 would be 0b00000011
    constexpr uint8_t setLowerBits(uint8_t bitCount) {
      return (1 << bitCount) - 1;
    }

    namespace details {
      template<class Visitor>
      void visit8n(size_t baseBit, uint8_t block, size_t bitCount, const Visitor& visitor) {
        if(block) {
          size_t i = 0;
          while(i < bitCount) {
            const size_t zeroes = std::countr_zero(block);
            if(zeroes) {
              block >>= zeroes;
              i += zeroes;
            }
            else {
              if constexpr(std::is_same_v<bool, decltype(visitor(size_t{}))>) {
                if(!visitor(baseBit + i)) {
                  return;
                }
              }
              else {
                visitor(baseBit + i);
              }
              block >>= 1;
              i += 1;
            }
          }
        }
      }

      template<class Visitor>
      void visit8(size_t baseBit, uint8_t block, const Visitor& visitor) {
        visit8n(baseBit, block, 8, visitor);
      }

      template<class Visitor>
      void visit16(size_t baseBit, const uint16_t& block, const Visitor& visitor) {
        if(block) {
          const uint8_t* sub = reinterpret_cast<const uint8_t*>(&block);
          visit8(baseBit, sub[0], visitor);
          visit8(baseBit + 8, sub[1], visitor);
        }
      }

      template<class Visitor>
      void visit32(size_t baseBit, const uint32_t& block, const Visitor& visitor) {
        if(block) {
          const uint16_t* sub = reinterpret_cast<const uint16_t*>(&block);
          visit16(baseBit, sub[0], visitor);
          visit16(baseBit + 16, sub[1], visitor);
        }
      }

      template<class Visitor>
      void visit64(size_t baseBit, const uint64_t& block ,const Visitor& visitor) {
        if(block) {
          const uint32_t* sub = reinterpret_cast<const uint32_t*>(&block);
          visit32(baseBit, sub[0], visitor);
          visit32(baseBit + 32, sub[1], visitor);
        }
      }
    }

    template<class Visitor>
    void visitSetBits(const uint8_t* buffer, size_t bitCount, const Visitor& visitor) {
      size_t remaining = bitCount;
      while(remaining >= 64) {
        details::visit64(bitCount - remaining, reinterpret_cast<const uint64_t&>(*buffer), visitor);
        buffer += 8;
        remaining -= 64;
      }

      if(remaining >= 32) {
        details::visit32(bitCount - remaining, reinterpret_cast<const uint32_t&>(*buffer), visitor);
        buffer += 4;
        remaining -= 32;
      }

      if(remaining >= 16) {
        details::visit16(bitCount - remaining, reinterpret_cast<const uint16_t&>(*buffer), visitor);
        buffer += 2;
        remaining -= 16;
      }

      if(remaining >= 8) {
        details::visit8(bitCount - remaining, reinterpret_cast<const uint8_t&>(*buffer), visitor);
        buffer += 1;
        remaining -= 8;
      }

      if(remaining) {
        details::visit8n(bitCount - remaining, *buffer, remaining, visitor);
      }
    }

    size_t seekNSetBit(const uint8_t* buffer, size_t bitCount, size_t startBit) {
      size_t result = bitCount;
      visitSetBits(buffer, bitCount, [&result, startBit](size_t setBit) {
        if(setBit >= startBit) {
          result = setBit;
          return false;
        }
        return true;
      });
      return result;
    }

    size_t seekSetBit(const uint8_t* buffer, size_t bitCount) {
      size_t result = bitCount;
      visitSetBits(buffer, bitCount, [&result](size_t setBit) {
        result = setBit;
        return false;
      });
      return result;
    }

    struct IndexedBit {
      IndexedBit& operator=(bool v) {
        if(v) {
          byte |= mask;
        }
        else {
          byte &= ~mask;
        }
        return *this;
      }

      explicit operator bool() const {
        return (byte & mask) != 0;
      }

      uint8_t& byte;
      uint8_t mask{};
    };

    struct ConstIndexedBit {
      explicit operator bool() const {
        return (byte & mask) != 0;
      }

      const uint8_t& byte;
      uint8_t mask{};
    };

    //Get the given byte corresponding to the index and a mask where only the desired bit is set
    IndexedBit indexBit(uint8_t* buffer, size_t bitIndex) {
      return IndexedBit{
        .byte = buffer[bitIndex/8],
        .mask = static_cast<uint8_t>(1 << (bitIndex % 8))
      };
    }

    ConstIndexedBit indexBit(const uint8_t* buffer, size_t bitIndex) {
      return ConstIndexedBit{
        .byte = buffer[bitIndex/8],
        .mask = static_cast<uint8_t>(1 << (bitIndex % 8))
      };
    }

    void memSetBits(uint8_t* buffer, bool value, size_t bitCount) {
      const size_t bytes = bitCount / 8;
      const uint8_t fillByte = value ? 255 : 0;
      std::memset(buffer, static_cast<int>(fillByte), bytes);
      if(const size_t remainder = bitCount % 8) {
        const uint8_t mask = setLowerBits(static_cast<uint8_t>(remainder));
        if(value) {
          buffer[bytes] |= mask;
        }
        else {
          buffer[bytes] &= ~mask;
        }
      }
    }
  };

  class DynamicBitset {
  public:
    class It {
    public:
      using value_type        = std::pair<size_t, bitops::IndexedBit>;
      using pointer           = value_type*;
      using reference         = value_type&;

      It(size_t curBit, size_t bitSize, uint8_t* buff)
        : currentBit{ curBit }
        , bitCount{ bitSize }
        , buffer{ buff }
      {
        //Skip to the first set bit if it didn't start on one
        if(currentBit < bitCount && !(**this).second) {
          ++*this;
        }
      }

      It& operator++() {
        const size_t startBit = currentBit + 1;
        const size_t startByte = startBit / 8;
        const size_t roundBit = startByte * 8;
        currentBit += (bitops::seekNSetBit(&buffer[startByte], bitCount - roundBit, startBit % 8) - roundBit);
        return *this;
      }

      It& operator++(int) {
        It tmp{ *this };
        ++*this;
        return *this;
      }

      value_type operator*() const {
        return std::make_pair(currentBit, bitops::indexBit(buffer, currentBit));
      }

      bool operator==(const It& rhs) const {
        return currentBit == rhs.currentBit;
      }

      bool operator!=(const It& rhs) const {
        return !(*this == rhs);
      }

    private:
      size_t currentBit{};
      size_t bitCount{};
      uint8_t* buffer{};
    };

    class ConstIt {
    public:
      using value_type        = std::pair<size_t, bitops::ConstIndexedBit>;
      using pointer           = value_type*;
      using reference         = value_type&;

      ConstIt(size_t curBit, size_t bitSize, const uint8_t* buff)
        : currentBit{ curBit }
        , bitCount{ bitSize }
        , buffer{ buff }
      {
        //Skip to the first set bit if it didn't start on one
        if(currentBit < bitCount && !(**this).second) {
          ++*this;
        }
      }

      ConstIt& operator++() {
        const size_t startBit = currentBit + 1;
        const size_t startByte = startBit / 8;
        const size_t roundBit = startByte * 8;
        currentBit += (bitops::seekNSetBit(&buffer[startByte], bitCount - roundBit, startBit % 8) - roundBit);
        return *this;
      }

      ConstIt& operator++(int) {
        ConstIt tmp{ *this };
        ++*this;
        return *this;
      }

      value_type operator*() const {
        return std::make_pair(currentBit, bitops::indexBit(buffer, currentBit));
      }

      bool operator==(const ConstIt& rhs) const {
        return currentBit == rhs.currentBit;
      }

      bool operator!=(const ConstIt& rhs) const {
        return !(*this == rhs);
      }

    private:
      size_t currentBit{};
      size_t bitCount{};
      const uint8_t* buffer{};
    };

    using iterator = It;
    using const_iterator = ConstIt;

    DynamicBitset() = default;

    DynamicBitset(const DynamicBitset& rhs) {
      resize(rhs.size());
      std::memcpy(getStorage(), rhs.getStorage(), bitops::bytesToContainBits(size()));
    }

    DynamicBitset(DynamicBitset&& rhs)
      : storage{ rhs.storage }
      , sizeBits{ rhs.sizeBits }
    {
      if(this != &rhs) {
        rhs.release();
      }
    }

    ~DynamicBitset() {
      deallocate();
      release();
    }

    DynamicBitset& operator=(const DynamicBitset& rhs) {
      DynamicBitset temp{ rhs };
      swap(temp);
      return *this;
    }

    DynamicBitset& operator=(DynamicBitset&& rhs) {
      DynamicBitset temp{ std::move(rhs) };
      swap(temp);
      return *this;
    }

    void swap(DynamicBitset& other) {
      std::swap(storage, other.storage);
      std::swap(sizeBits, other.sizeBits);
    }

    void resize(size_t newSize) {
      reallocate(newSize);
    }

    size_t size() const {
      return sizeBits;
    }

    //Only iterates over bits that are set
    iterator begin() {
      return { 0, size(), getStorage() };
    }

    iterator end() {
      return { size(), size(), nullptr };
    }

    const_iterator begin() const {
      return { 0, size(), getStorage() };
    }

    const_iterator end() const {
      return { size(), size(), nullptr };
    }

    //Reset all bits to zero. Avoiding "clear" as that would be a bit confusing for size change
    void resetBits() {
      bitops::memSetBits(getStorage(), false, size());
    }

    void setAllBits() {
      bitops::memSetBits(getStorage(), true, size());
    }

    bool any() const {
      bool r{};
      bitops::visitSetBits(getStorage(), size(), [&r](size_t) {
        r = true;
        return false;
      });
      return r;
    }

    bool none() const {
      bool r = true;
      bitops::visitSetBits(getStorage(), size(), [&r](size_t) {
        r = false;
        return false;
      });
      return r;
    }

    bool test(size_t i) const {
      bitops::ConstIndexedBit b = bitops::indexBit(getStorage(), i);
      return (b.byte & b.mask) != 0;
    }

    void set(size_t i) {
      bitops::IndexedBit b = bitops::indexBit(getStorage(), i);
      b.byte |= b.mask;
    }

    void set(size_t i, bool value) {
      if(value) {
        set(i);
      }
      else {
        bitops::IndexedBit b = bitops::indexBit(getStorage(), i);
        b.byte &= ~b.mask;
      }
    }

  private:
    static constexpr size_t IN_PLACE_STORAGE = sizeof(uint8_t*)*8;
    bool hasAllocatedStorage() const {
      return sizeBits > IN_PLACE_STORAGE;
    }

    void deallocate() {
      if(hasAllocatedStorage()) {
        delete [] storage;
      }
      storage = nullptr;
    }

    void release() {
      storage = nullptr;
      sizeBits = 0;
    }

    void reallocate(size_t newSize) {
      deallocate();
      if(newSize > IN_PLACE_STORAGE) {
        storage = new uint8_t[bitops::bytesToContainBits(newSize)](0);
      }
      sizeBits = newSize;
    }

    //Use the pointer as storage itself if the bit count is small enough
    uint8_t* getStorage() {
      return hasAllocatedStorage() ? storage : reinterpret_cast<uint8_t*>(&storage);
    }

    const uint8_t* getStorage() const {
      return hasAllocatedStorage() ? storage : reinterpret_cast<const uint8_t*>(&storage);
    }

    uint8_t* storage{};
    size_t sizeBits{};
  };
}