#pragma once

struct PhysicsConfig {
  std::optional<size_t> mForcedTargetWidth;
  float linearDragMultiplier = 0.96f;
  float angularDragMultiplier = 0.99f;
  bool drawCollisionPairs{};
  bool drawContacts{};
  int solveIterations = 5;
  float frictionCoeff = 0.5f;
};
