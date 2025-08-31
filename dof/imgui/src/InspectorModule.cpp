#include "Precompile.h"
#include "InspectorModule.h"

#include "config/Config.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "AppBuilder.h"
#include "ImguiModule.h"

#include "DebugDrawer.h"
#include "Narrowphase.h"
#include "PhysicsSimulation.h"
#include "SpatialPairsStorage.h"
#include "ThreadLocals.h"
#include "Random.h"
#include <math/Geometric.h>
#include "Fragment.h"
#include "generics/IntMath.h"
#include "TableName.h"
#include <transform/TransformModule.h>

namespace InspectorModule {
  struct InspectContext {
    InspectContext(RuntimeDatabaseTaskBuilder& task)
      : ids{ task.getIDResolver() }
      , resolver{ task.getResolver(
          transform, transformUpdate,
          thickness, sharedThickness,
          collisionMask,
          velX, velY, velZ, velA,
          tableNames,
          goalX, goalY,
          goalFound,
          seekingGoal,
          camera
        )
      }
      , refResolver{ ids->getRefResolver() }
      , playerIds{ task.query<const StableIDRow, const IsPlayer>().get<0>() }
      , stableRows{ task.query<const StableIDRow>().get<0>() }
      , data{ task.query<InspectorRow>().tryGetSingletonElement() }
      , debugLines{ TableAdapters::getDebugLines(task) }
      , fragmentMigrator{ Fragment::createFragmentMigrator(task) }
      , fragmentIds{ task.query<const StableIDRow, const IsFragment>().get<0>() }
      , cameraIds{ task.query<const StableIDRow, const Row<Camera>>().get<0>() }
    {
    }

    explicit operator bool() const {
      return data != nullptr;
    }

    static std::string getPrettyName(size_t stableId, const char* tableName) {
      return "[" + std::to_string(stableId) + "] " + tableName;
    }

    std::string getPrettyName(const Selection& obj) {
      auto unpacked = refResolver.uncheckedUnpack(obj.ref);
      if(const auto* name = resolver->tryGetOrSwapRowElement(tableNames, unpacked)) {
        return getPrettyName(obj.stableID, name->name.c_str());
      }
      return getPrettyName(obj.stableID, ("Table " + std::to_string(unpacked.getTableIndex())).c_str());
    }

    std::shared_ptr<IIDResolver> ids;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver refResolver;
    CachedRow<Transform::WorldTransformRow> transform;
    CachedRow<Transform::TransformNeedsUpdateRow> transformUpdate;
    CachedRow<Narrowphase::ThicknessRow> thickness;
    CachedRow<Narrowphase::SharedThicknessRow> sharedThickness;
    CachedRow<Narrowphase::CollisionMaskRow> collisionMask;
    CachedRow<Tags::LinVelXRow> velX;
    CachedRow<Tags::LinVelYRow> velY;
    CachedRow<Tags::LinVelZRow> velZ;
    CachedRow<Tags::AngVelRow> velA;
    CachedRow<const TableName::TableNameRow> tableNames;
    CachedRow<const Tags::FragmentGoalXRow> goalX;
    CachedRow<const Tags::FragmentGoalYRow> goalY;
    CachedRow<const FragmentGoalFoundTableTag> goalFound;
    CachedRow<const FragmentSeekingGoalTagRow> seekingGoal;
    CachedRow<Row<Camera>> camera;
    std::vector<const StableIDRow*> playerIds;
    std::vector<const StableIDRow*> stableRows;
    std::vector<const StableIDRow*> fragmentIds;
    std::vector<const StableIDRow*> cameraIds;
    std::shared_ptr<Fragment::IFragmentMigrator> fragmentMigrator;
    InspectorData* data{};
    DebugLineAdapter debugLines;
    AppTaskArgs* args{};
    glm::vec2 pos{};
    glm::vec2 scale{};
    ElementRef self;
  };

  void presentFragmentFields(InspectContext& ctx, const UnpackedDatabaseElementID& self) {
    const size_t i = self.getElementIndex();
    if(ctx.resolver->tryGetOrSwapAllRows(self, ctx.goalX, ctx.goalY)) {
      std::array value{ ctx.goalX->at(i), ctx.goalY->at(i) };
      //Read-only
      ImGui::InputFloat2("Goal", value.data());
    }

    const bool seekingGoal = ctx.resolver->tryGetOrSwapRow(ctx.seekingGoal, self);
    bool foundGoal = ctx.resolver->tryGetOrSwapRow(ctx.goalFound, self);
    if(seekingGoal || foundGoal) {
      if(ImGui::Checkbox("Goal Found", &foundGoal)) {
        if(foundGoal) {
          ctx.fragmentMigrator->moveActiveToComplete(ctx.self, *ctx.args);
        }
        else {
          ctx.fragmentMigrator->moveCompleteToActive(ctx.self, *ctx.args);
        }
      }
    }
  }

  void presentCamera(InspectContext& ctx, const UnpackedDatabaseElementID& self) {
    if(Camera* cam = ctx.resolver->tryGetOrSwapRowElement(ctx.camera, self)) {
      ImGui::InputFloat("Zoom", &cam->zoom, 0.1f, 0.5f);
    }
  }

  bool presentOne(InspectContext& ctx, const Selection& obj) {
    auto unpacked = ctx.refResolver.tryUnpack(obj.ref);
    if(!unpacked) {
      return false;
    }
    ctx.pos = {};
    ctx.self = obj.ref;

    const size_t i = unpacked->getElementIndex();

    ImGui::PushID(obj.ref.unversionedHash());
    {
      ImGui::Text(ctx.getPrettyName(obj).c_str());
      if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.transform, ctx.transformUpdate)) {
        Transform::PackedTransform& transform = ctx.transform->at(i);
        auto flagUpdate = [&] { ctx.transformUpdate->getOrAdd(i); };

        std::array value{ transform.tx, transform.ty, transform.tx };
        if(ImGui::InputFloat3("Position", value.data())) {
          transform.tx = value[0];
          transform.ty = value[1];
          transform.tz = value[2];
          flagUpdate();
        }
        const glm::vec2 rot = transform.rot();
        float angle = Constants::RADDEG * std::atan2f(rot.y, rot.x);
        if(ImGui::InputFloat("Rotation", &angle)) {
          transform.setRot(Constants::DEGRAD * angle);
          flagUpdate();
        }
        ctx.scale = transform.scale();
        if(ImGui::InputFloat2("Scale", &ctx.scale.x)) {
          transform.setScale(ctx.scale);
          flagUpdate();
        }
      }

      if(float* thickness = ctx.resolver->tryGetOrSwapRowElement(ctx.thickness, *unpacked)) {
        ImGui::InputFloat("Thickness", thickness);
      }
      if(ctx.resolver->tryGetOrSwapRow(ctx.sharedThickness, *unpacked)) {
        ImGui::InputFloat("Shared Thickness", &ctx.sharedThickness->at());
      }

      if(Narrowphase::CollisionMask* mask = ctx.resolver->tryGetOrSwapRowElement(ctx.collisionMask, *unpacked)) {
        if(ImGui::TreeNode("Collision Mask")) {
          constexpr std::array<std::string_view, 8> names = {
            std::string_view("Default"),
            std::string_view("Path"),
            std::string_view("?"),
            std::string_view("?"),
            std::string_view("?"),
            std::string_view("?"),
            std::string_view("?"),
            std::string_view("?")
          };
          for (int b = 0; b < 8; ++b) {
            ImGui::PushID(b);
            const uint8_t bit = (1 << b);
            bool enabled = (*mask) & bit;
            if(ImGui::Checkbox(names[b].data(), &enabled)) {
              *mask &= ~bit;
              if(enabled) {
                *mask |= bit;
              }
            }
            ImGui::PopID();
          }
          int iMask = static_cast<int>(*mask);
          if(ImGui::CheckboxFlags("All", &iMask, 255)) {
            *mask = static_cast<uint8_t>(iMask);
          }
          ImGui::TreePop();
        }
      }
    }

    presentFragmentFields(ctx, *unpacked);
    presentCamera(ctx, *unpacked);

    ImGui::PopID();
    return true;
  }

  void setSelection(InspectContext& ctx, const ElementRef& value, size_t stableId) {
    if(ctx.data->selected.empty()) {
      ctx.data->selected.resize(1);
    }
    ctx.data->selected[0] = { value, stableId };
  }

  void findAndSetSelection(InspectContext& ctx, const ElementRef& value) {
    size_t i = 0;
    for(const auto* stableRow : ctx.stableRows) {
      for(const auto& id : *stableRow) {
        if(id == value) {
          setSelection(ctx, value, i);
        }
        ++i;
      }
    }
  }

  void clearSelection(InspectContext& ctx) {
    ctx.data->selected.clear();
  }

  void trySelect(InspectContext& ctx, const std::vector<const StableIDRow*>& rows) {
    for(const auto& row : rows) {
      for(const ElementRef& id : *row) {
        if(id) {
          findAndSetSelection(ctx, id);
          return;
        }
      }
    }
  }

  void presentPicker(InspectContext& ctx) {
    //Selection by stable id incrementing tries to find the "next" one
    int raw{};
    if(ctx.data->selected.size() && ctx.data->selected[0].ref) {
      raw = static_cast<int>(ctx.data->selected[0].stableID);
    }
    const int prev = raw;
    if(ImGui::InputInt("Selected", &raw) && raw != prev) {
      int direction = raw > prev ? 1 : -1;
      int searched = 0;
      ElementRef found;
      //TODO: horribly inefficient, maybe doesn't matter
      //TODO: also doesn't really work for stable iteration
      std::vector<ElementRef> all;
      for(const StableIDRow* r : ctx.stableRows) {
        all.insert(all.end(), r->begin(), r->end());
      }
      //Start at the selected id then keep going in that direction until something is found or all ids are exhausted
      const int totalIds = all.size();
      raw = gnx::IntMath::wrap(raw, totalIds);

      while(searched++ < totalIds) {
        found = all[raw];
        if(found) {
          break;
        }

        //Increment with wrap
        raw += direction;
        if(raw >= totalIds) {
          raw = 0;
        }
        else if(raw < 0) {
          raw = totalIds - 1;
        }
      }

      if(found) {
        setSelection(ctx, found, static_cast<size_t>(raw));
      }
      else {
        clearSelection(ctx);
      }
    }
    if(ImGui::Button("Select Player")) {
      trySelect(ctx, ctx.playerIds);
    }
    if(ImGui::Button("Select Fragment")) {
      trySelect(ctx, ctx.fragmentIds);
    }
    if(ImGui::Button("Select Camera")) {
      trySelect(ctx, ctx.cameraIds);
    }
    if(ImGui::Button("Clear Selection")) {
      clearSelection(ctx);
    }
  }

  void highlightSelected(InspectContext& ctx) {
    for(const Selection& selection : ctx.data->selected) {
      const auto unpacked = ctx.refResolver.tryUnpack(selection.ref);
      if(!unpacked) {
        continue;
      }
      const size_t i = unpacked->getElementIndex();
      if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.transform)) {
        //Draw a little box at the center of the object
        const glm::vec2 pos = ctx.transform->at(i).pos2();
        const glm::vec2 s = ctx.transform->at(i).scale();
        DebugDrawer::drawAABB(ctx.debugLines, pos - s, pos + s, { 0, 1, 0 });
      }
    }
  }

  void update(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Inspector").setPinning(AppTaskPinning::MainThread{});
    InspectContext inspector{ task };
    if(!inspector) {
      task.discard();
      return;
    }
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    task.setCallback([inspector, enabled](AppTaskArgs& args) mutable {
      if(!*enabled) {
        return;
      }
      inspector.args = &args;
      ImGui::Begin("Inspector");
      {
        presentPicker(inspector);
        for(size_t i = 0; i < inspector.data->selected.size();) {
          //Present this if it is valid, otherwise remove it from the selected list
          if(presentOne(inspector, inspector.data->selected[i])) {
            ++i;
          }
          else {
            inspector.data->selected.erase(inspector.data->selected.begin() + i);
          }
        }
        highlightSelected(inspector);
      }
      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }
}