#include "Precompile.h"
#include "ecs/system/editor/ObjectInspectorSystem.h"

const char* ObjectInspectorSystemBase::WINDOW_NAME = "Inspector";
const char* ObjectInspectorSystemBase::COMPONENT_LIST = "ScrollView";
const char* ObjectInspectorSystemBase::ADD_COMPONENT_BUTTON = "Add Component";
const char* ObjectInspectorSystemBase::REMOVE_COMPONENT_BUTTON = "Remove Component";
const char* ObjectInspectorSystemBase::COMPONENT_PICKER_MODAL = "Components";

std::shared_ptr<Engine::System> ObjectInspectorSystemBase::init() {
  using namespace Engine;
  return ecx::makeSystem("InitObjectInspector", [](SystemContext<EntityFactory>& context) {
    context.get<EntityFactory>().
      createEntityWithComponents<
        ObjectInspectorContextComponent,
        PickerContextComponent>();
  });
}
