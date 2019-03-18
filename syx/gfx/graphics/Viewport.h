#pragma once

class Viewport {
public:
  Viewport(std::string name, const Syx::Vec2& min, const Syx::Vec2& max);

  const std::string& getName() const;
  Syx::Vec2 getMin() const;
  Syx::Vec2 getMax() const;

  void set(const Syx::Vec2& min, const Syx::Vec2& max);

private:
  std::string mName;
  Syx::Vec2 mMin;
  Syx::Vec2 mMax;
};