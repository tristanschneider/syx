#pragma once

namespace Config {
  struct CurveConfig {
    std::optional<float> scale;
    std::optional<float> offset;
    std::optional<float> duration;
    bool flipInput{};
    bool flipOutput{};
    std::string curveFunction;
  };

  struct PhysicsConfig {
    std::optional<size_t> mForcedTargetWidth;
    float linearDragMultiplier = 0.96f;
    float angularDragMultiplier = 0.99f;
    bool drawCollisionPairs{};
    bool drawContacts{};
    int solveIterations = 5;
    float frictionCoeff = 0.5f;
  };

  struct PlayerConfig {
    bool drawMove{};
    CurveConfig linearSpeedCurve;
    CurveConfig linearForceCurve;
    CurveConfig angularSpeedCurve;
    CurveConfig angularForceCurve;
    CurveConfig linearStoppingSpeedCurve;
    CurveConfig linearStoppingForceCurve;
    CurveConfig angularStoppingSpeedCurve;
    CurveConfig angularStoppingForceCurve;
  };

  struct PlayerAbilityConfig {
    size_t explodeLifetime = 5;
    float explodeStrength = 0.05f;
  };

  struct CameraConfig {
    float cameraZoomSpeed = 0.3f;
    float zoom = 1.0f;
    CurveConfig followCurve;
  };

  struct FragmentConfig {
    float fragmentGoalDistance = 0.5f;

    size_t fragmentRows = 10;
    size_t fragmentColumns = 9;
  };

  struct WorldConfig {
    float deltaTime = 1.0f/60.0f;
    float boundarySpringConstant = 0.01f;
  };

  struct RawGameConfig {
    PlayerConfig player;
    PlayerAbilityConfig ability;
    CameraConfig camera;
    FragmentConfig fragment;
    WorldConfig world;
    PhysicsConfig physics;
  };
}