#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "SyxVec2.h"
#include "event/BaseComponentEvents.h"
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaState.h"
#include "lua/LuaVariant.h"

#include "test/MockApp.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(SceneSaveLoadTests) {
  public:
  };
}