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
#include "Geometric.h"

namespace InspectorModule {
  struct InspectContext {
    InspectContext(RuntimeDatabaseTaskBuilder& task)
      : ids{ task.getIDResolver() }
      , resolver{ task.getResolver(
          posX, posY, posZ,
          rotX, rotY,
          velX, velY, velZ, velA,
          tableNames
        )
      }
      , refResolver{ ids->getRefResolver() }
      , playerIds{ task.query<const StableIDRow, const IsPlayer>().get<0>() }
      , data{ task.query<InspectorRow>().tryGetSingletonElement() }
      , debugLines{ TableAdapters::getDebugLines(task) }
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
    CachedRow<Tags::PosXRow> posX;
    CachedRow<Tags::PosYRow> posY;
    CachedRow<Tags::PosZRow> posZ;
    CachedRow<Tags::RotXRow> rotX;
    CachedRow<Tags::RotYRow> rotY;
    CachedRow<Tags::LinVelXRow> velX;
    CachedRow<Tags::LinVelYRow> velY;
    CachedRow<Tags::LinVelZRow> velZ;
    CachedRow<Tags::AngVelRow> velA;
    CachedRow<const Tags::TableNameRow> tableNames;
    std::vector<const StableIDRow*> playerIds;
    InspectorData* data{};
    DebugLineAdapter debugLines;
  };

  bool presentOne(InspectContext& ctx, const Selection& obj) {
    auto unpacked = ctx.refResolver.tryUnpack(obj.ref);
    if(!unpacked) {
      return false;
    }
    const size_t i = unpacked->getElementIndex();

    ImGui::PushID(obj.ref.unversionedHash());
    {
      ImGui::Text(ctx.getPrettyName(obj).c_str());
      if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.posX, ctx.posY, ctx.posZ)) {
        std::array value{ ctx.posX->at(i), ctx.posY->at(i), ctx.posZ->at(i) };
        if(ImGui::InputFloat3("Position", value.data())) {
          ctx.posX->at(i) = value[0];
          ctx.posY->at(i) = value[1];
          ctx.posZ->at(i) = value[2];
        }
      }
      else if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.posX, ctx.posY)) {
        std::array value{ ctx.posX->at(i), ctx.posY->at(i) };
        if(ImGui::InputFloat2("Position", value.data())) {
          ctx.posX->at(i) = value[0];
          ctx.posY->at(i) = value[1];
        }
      }
      if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.rotX, ctx.rotY)) {
        float angle = Geo::RADDEG * std::atan2f(ctx.rotY->at(i), ctx.rotX->at(i));
        if(ImGui::InputFloat("Rotation", &angle)) {
          const float rad = Geo::DEGRAD * angle;
          ctx.rotX->at(i) = std::cos(rad);
          ctx.rotY->at(i) = std::sin(rad);
        }
      }
    }
    ImGui::PopID();
    return true;
  }

  void setSelection(InspectContext& ctx, const ElementRef& value, size_t stableId) {
    if(ctx.data->selected.empty()) {
      ctx.data->selected.resize(1);
    }
    ctx.data->selected[0] = { value, stableId };
  }

  void clearSelection(InspectContext& ctx) {
    ctx.data->selected.clear();
  }

  void trySelectPlayer(InspectContext& ctx) {
    for(const auto& row : ctx.playerIds) {
      for(const size_t id : row->mElements) {
        if(const ElementRef found = ctx.ids->tryResolveRef(StableElementID::fromStableID(id))) {
          setSelection(ctx, found, id);
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
      //Start at the selected id then keep going in that direction until something is found or all ids are exhausted
      const int totalIds = static_cast<int>(ctx.ids->getTotalIds());
      while(searched++ < totalIds) {
        found = ctx.ids->tryResolveRef(StableElementID::fromStableID(static_cast<size_t>(raw)));
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
      trySelectPlayer(ctx);
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
      if(ctx.resolver->tryGetOrSwapAllRows(*unpacked, ctx.posX, ctx.posY)) {
        //Draw a little box at the center of the object
        const glm::vec2 pos = TableAdapters::read(i, *ctx.posX, *ctx.posY);
        const glm::vec2 s{ 0.25f };
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
    task.setCallback([inspector, enabled](AppTaskArgs&) mutable {
      if(!*enabled) {
        return;
      }
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