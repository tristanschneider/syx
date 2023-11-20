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

  template<class T>
  struct IAdapter {
    virtual ~IAdapter() = default;
    virtual T read() const = 0;
    virtual void write(const T&) = 0;
  };

  using ICurveAdapter = IAdapter<CurveConfig>;

  //Virtual here is a hacky way to get around dependency issue where this library describes the values
  //for a cuve without knowing what a curve is but adapter can update its infurmation whenever the base config changes
  template<class Adapter>
  struct ConfigExt {
    std::unique_ptr<Adapter> adapter;
  };
  using CurveConfigExt = ConfigExt<ICurveAdapter>;

  struct AbilityTriggerConfig {
    std::string type;
    float minCharge{};
    CurveConfig chargeCurve;
    CurveConfig damageCurve;
  };

  struct AbilityCooldownConfig {
    std::string type;
    float maxTime{};
  };

  struct AbilityConfig {
    AbilityTriggerConfig trigger;
    AbilityCooldownConfig cooldown;
  };

  using IAbilityAdapter = IAdapter<AbilityConfig>;
  using AbilityConfigExt = ConfigExt<IAbilityAdapter>;

  struct IFactory {
    virtual ~IFactory() = default;
    virtual void init(CurveConfigExt& curve) const = 0;
    virtual void init(AbilityConfigExt& curve) const = 0;
  };

  struct PhysicsConfig {
    std::optional<size_t> mForcedTargetWidth;
    float linearDragMultiplier = 0.96f;
    float angularDragMultiplier = 0.99f;
    bool drawCollisionPairs{};
    bool drawContacts{};
    int solveIterations = 5;
    float frictionCoeff = 0.5f;

    struct Broadphase {
      float bottomLeftX = -200.0f;
      float bottomLeftY = -200.0f;
      float cellSizeX = 20.0f;
      float cellSizeY = 20.0f;
      float cellPadding = 0.0f;
      size_t cellCountX = 20;
      size_t cellCountY = 20;
      bool draw{};
    };
    Broadphase broadphase;
  };

  struct PlayerConfig {
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

  struct PushAbility {
    AbilityConfigExt ability;
    float dynamicPiercing = 3.0f;
    float terrainPiercing = 0.0f;
    float coneHalfAngle = 0.25f;
    float coneLength = 15.f;
    size_t rayCount = 4;
  };

  struct PlayerAbilityConfig {
    PushAbility pushAbility;
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
    bool drawAI{};
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