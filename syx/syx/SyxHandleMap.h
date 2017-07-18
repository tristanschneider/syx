#pragma once

//This is the same as DeclareIntrusiveNode, but using this so I'm not bound to intrusive if I want to change
#define DeclareHandleMapNode(ClassType) IntrusiveNode<ClassType> mIntrusiveNode;

namespace Syx {
  template <typename Value>
  //Maps keys to values in a page based manner such that iteration is as good as contiguous, and addresses don't change
  class HandleMap {
  public:
    class Iterator {
    public:
      Iterator(IntrusiveIterator<Value> it): m_it(it) {}
      Value& operator*(void) { return *m_it; }
      Value* DataPointer(void) { return m_it.DataPointer(); }
      Iterator& operator++(void) { ++m_it; return *this; }
      Iterator operator++(int) { return Iterator(m_it++); }
      Iterator& operator--(void) { --m_it; return *this; }
      Iterator operator--(int) { return Iterator(m_it--); }
      bool operator==(const Iterator& rhs) { return m_it == rhs.m_it; }
      bool operator!=(const Iterator& rhs) { return m_it != rhs.m_it; }

    private:
      IntrusiveIterator<Value> m_it;
    };

    HandleMap(): mNewKey(static_cast<Handle>(0)) {}

    Iterator Begin() {
      return mValueStore.Begin();
    }

    Iterator End() {
      return mValueStore.End();
    }

    size_t Size() {
      return mValueStore.Size();
    }

    Value* Get(Handle key) const {
      auto it = mKeyToValue.find(key);
      if(it != mKeyToValue.end())
        return it->second;
      return nullptr;
    }

    Value* Add() {
      Handle newKey = mKeygen.Next();

      Value* result = mValueStore.Push(Value(newKey));

      //Could assert here for duplicate insertion
      mKeyToValue[newKey] = result;
      return result;
    }

    void Remove(Handle key) {
      auto it = mKeyToValue.find(key);
      if(it == mKeyToValue.end())
        return;

      mValueStore.Free(it->second);
      mKeyToValue.erase(it);
    }

    void Clear() {
      mKeyToValue.clear();
      mValueStore.Clear();
      mKeygen.Reset();
    }

    void Reserve(size_t size) {
      mKeyToValue.reserve(size);
    }

  private:
    VecList<Value> mValueStore;
    std::unordered_map<Handle, Value*> mKeyToValue;
    Handle mNewKey;
    HandleGenerator mKeygen;
  };
}