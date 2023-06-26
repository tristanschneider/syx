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

  struct ICurveAdapter {
    virtual ~ICurveAdapter() = default;
    virtual CurveConfig read() const = 0;
    virtual void write(const CurveConfig&) = 0;
  };

  //Virtual here is a hacky way to get around dependency issue where this library describes the values
  //for a cuve without knowing what a curve is but adapter can update its infurmation whenever the base config changes
  struct CurveConfigExt {
    std::unique_ptr<ICurveAdapter> adapter;
  };

  struct IFactory {
    virtual ~IFactory() = default;
    virtual CurveConfigExt createCurve() const = 0;
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
    void init(const IFactory& factory) {
      linearSpeedCurve = factory.createCurve();
      linearForceCurve = factory.createCurve();
      angularSpeedCurve = factory.createCurve();
      angularForceCurve = factory.createCurve();
      linearStoppingSpeedCurve = factory.createCurve();
      linearStoppingForceCurve = factory.createCurve();
      angularStoppingSpeedCurve = factory.createCurve();
      angularStoppingForceCurve = factory.createCurve();
    }

    bool drawMove{};
    CurveConfigExt linearSpeedCurve;
    CurveConfigExt linearForceCurve;
    CurveConfigExt angularSpeedCurve;
    CurveConfigExt angularForceCurve;
    CurveConfigExt linearStoppingSpeedCurve;
    CurveConfigExt linearStoppingForceCurve;
    CurveConfigExt angularStoppingSpeedCurve;
    CurveConfigExt angularStoppingForceCurve;
  };

  struct PlayerAbilityConfig {
    size_t explodeLifetime = 5;
    float explodeStrength = 0.05f;
  };

  struct CameraConfig {
    float cameraZoomSpeed = 0.3f;
    float zoom = 1.0f;
    CurveConfigExt followCurve;
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

  struct GameConfig {
    PlayerConfig player;
    PlayerAbilityConfig ability;
    CameraConfig camera;
    FragmentConfig fragment;
    WorldConfig world;
    PhysicsConfig physics;
  };
}