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
      Value* dataPointer(void) { return m_it.dataPointer(); }
      Iterator& operator++(void) { ++m_it; return *this; }
      Iterator operator++(int) { return Iterator(m_it++); }
      Iterator& operator--(void) { --m_it; return *this; }
      Iterator operator--(int) { return Iterator(m_it--); }
      bool operator==(const Iterator& rhs) { return m_it == rhs.m_it; }
      bool operator!=(const Iterator& rhs) { return m_it != rhs.m_it; }

    private:
      IntrusiveIterator<Value> m_it;
    };

    Iterator begin() {
      return mValueStore.begin();
    }

    Iterator end() {
      return mValueStore.end();
    }

    size_t size() {
      return mValueStore.size();
    }

    Value* get(Handle key) const {
      auto it = mKeyToValue.find(key);
      if(it != mKeyToValue.end())
        return it->second;
      return nullptr;
    }

    Value* add() {
      Handle newKey = mKeygen.next();

      Value* result = mValueStore.push(Value(newKey));

      //Could assert here for duplicate insertion
      mKeyToValue[newKey] = result;
      return result;
    }

    void remove(Handle key) {
      auto it = mKeyToValue.find(key);
      if(it == mKeyToValue.end())
        return;

      mValueStore.freeObj(it->second);
      mKeyToValue.erase(it);
    }

    void clear() {
      mKeyToValue.clear();
      mValueStore.clear();
      mKeygen.reset();
    }

    void reserve(size_t size) {
      mKeyToValue.reserve(size);
    }

  private:
    VecList<Value> mValueStore;
    std::unordered_map<Handle, Value*> mKeyToValue;
    HandleGenerator mKeygen;
  };
}