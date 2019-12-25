#include "Precompile.h"
#include "ScratchPad.h"

#include "util/Variant.h"

ScratchPad::ScratchPad(uint8_t defaultLifetime)
  : mDefaultLifetime(defaultLifetime) {
}

void ScratchPad::update() {
  for(auto it = mData.begin(); it != mData.end();) {
    std::pair<Variant, uint8_t>& pair = it->second;
    if(!pair.second) {
      it = mData.erase(it);
    }
    else {
      --pair.second;
      ++it;
    }
  }
}

void ScratchPad::clear() {
  mData.clear();
  mKey.clear();
  while(mKeyStack.size())
    mKeyStack.pop();
}

void ScratchPad::push(std::string_view key) {
  mKeyStack.push(mKey.size());
  mKey += key;
}

void ScratchPad::pop() {
  assert(!mKeyStack.empty() && "Nothing to pop");
  mKey.resize(mKeyStack.top());
  mKeyStack.pop();
}

Variant* ScratchPad::read(std::string_view key, bool refreshLifetime) {
  auto it = mData.find(_getKeyHash(key));
  if(it != mData.end()) {
    if(refreshLifetime)
      it->second.second = mDefaultLifetime;
    return &it->second.first;
  }
  return nullptr;
}

void ScratchPad::write(std::string_view key, Variant value) {
  size_t keyHash = _getKeyHash(key);
  auto it = mData.find(keyHash);
  if(it != mData.end()) {
    it->second.second = mDefaultLifetime;
    it->second.first = std::move(value);
  }
  else
    mData[keyHash] = { std::move(value), mDefaultLifetime };
}

size_t ScratchPad::_getKeyHash(std::string_view key) {
  push(key);
  size_t result = _getKeyHash();
  pop();
  return result;
}

size_t ScratchPad::_getKeyHash() const {
  return std::hash<std::string>()(mKey);
}
