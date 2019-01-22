#pragma once

class Component;

class ComponentRegistryProvider {
public:
  virtual void forEachComponentType(const std::function<void(const Component&)>& callback) const = 0;
};