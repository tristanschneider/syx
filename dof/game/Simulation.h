#pragma once

#include "Table.h"
#include "Config.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "StableElementID.h"
#include "Scheduler.h"
#include "DBEvents.h"
#include "RowTags.h"

class IAppBuilder;

//1 if found, otherwise 0. Bitset or better would be nice but interop with ispc would be difficult
struct FragmentGoalFoundRow : Row<uint8_t> {};

enum class RequestStatus : uint8_t {
  InProgress,
  Failed,
  Succeeded
};

struct CubeSprite {
  float uMin = 0;
  float vMin = 0;
  float uMax = 1;
  float vMax = 1;
};

struct SceneState {
  glm::vec2 mBoundaryMin{};
  glm::vec2 mBoundaryMax{};
};

struct FileSystem {
  std::string mRoot;
};

struct ThreadLocals;
struct ThreadLocalsInstance {
  ThreadLocalsInstance();
  ~ThreadLocalsInstance();

  std::unique_ptr<ThreadLocals> instance;
};

struct ThreadLocalsRow : SharedRow<ThreadLocalsInstance> {};

struct IsImmobile : TagRow{};
struct IsFragment : TagRow{};
struct DamageTaken : Row<float>{};
struct Tint : Row<glm::vec4>{};
enum class FragmentFlags : uint8_t {
  InBounds = 1 << 0,
};

struct SharedMassObjectTableTag : TagRow{};

struct FragmentFlagsRow : Row<FragmentFlags>{};

struct TargetTableTag : TagRow{};

struct Camera {
  float angle{};
  float zoom{};
  float fovDeg = 90.0f;
  float nearPlane = 0.1f;
  float farPlane = 100.0f;
  //Perspective if false
  bool orthographic = false;
};

struct DebugPoint {
  glm::vec2 mPos;
  glm::vec3 mColor;
};

struct DebugText {
  glm::vec2 pos;
  std::string text;
};

struct DebugClearPerFrame : TagRow{};

using DebugLineTable = Table<Row<DebugPoint>, DebugClearPerFrame>;
using DebugTextTable = Table<Row<DebugText>, DebugClearPerFrame>;

namespace Simulation {
  void initScheduler(IAppBuilder& builder);
  void init(IAppBuilder& builder);

  struct UpdateConfig {
    using Injector = std::function<void(IAppBuilder&)>;
    struct NoOp {
      operator Injector() const {
        return [](IAppBuilder&) { };
      }
    };

    //Soon after gameplay extract and physics started, before stat processing
    Injector injectGameplayTasks{ NoOp{} };
    Injector preEvents{ NoOp{} };
    bool enableFragmentStateMachine{ true };
  };

  void buildUpdateTasks(IAppBuilder& builder, const UpdateConfig& config);

  const char* getConfigName();
};