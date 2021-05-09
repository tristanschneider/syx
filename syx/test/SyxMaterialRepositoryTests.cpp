#include "Precompile.h"
#include "CppUnitTest.h"

#include <SyxMaterial.h>
#include <SyxMaterialRepository.h>
#include <SyxPhysicsSystem.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Syx;

namespace SyxTests {
  TEST_CLASS(MaterialRepository) {
    TEST_METHOD(SingleMaterialAlive_GarbageCollect_NothingDeleted) {
      auto repo = Create::defaultMaterialRepository();
      auto mat = repo->addMaterial({});

      Assert::AreEqual(repo->garbageCollect(), size_t(0), L"Nothing should be deleted because handle is still alive", LINE_INFO());
    }

    TEST_METHOD(SingleMaterialDead_GarbageCollect_OneDeleted) {
      auto repo = Create::defaultMaterialRepository();
      repo->addMaterial({});

      Assert::AreEqual(repo->garbageCollect(), size_t(1), L"Expired material should have been deleted", LINE_INFO());
    }

    struct PhysicsSystemWithMaterials {
      PhysicsSystemWithMaterials() {
        auto repo = Create::defaultMaterialRepository();
        mRepository = repo.get();
        mSystem = std::make_unique<PhysicsSystem>(std::move(repo));
      }

      IMaterialRepository* mRepository = nullptr;
      std::unique_ptr<PhysicsSystem> mSystem;
    };

    TEST_METHOD(SingleMaterialDeadButInUse_GarbageCollect_NoneDeleted) {
      PhysicsSystemWithMaterials system;
      auto mat = system.mRepository->addMaterial({});
      auto mod = std::make_shared<Syx::Model>(Syx::ModelType::Cube);
      auto space = system.mSystem->createSpace();
      system.mSystem->addPhysicsObject(false, true, space->_getHandle(), *mat, mod);

      mat.reset();
      Assert::AreEqual(system.mRepository->garbageCollect(), size_t(0), L"Material should still be alive because a physics object is using it", LINE_INFO());
    }

    TEST_METHOD(SingleMaterialDeadAndNotInUse_GarbageCollect_OneDeleted) {
      PhysicsSystemWithMaterials system;
      auto mat = system.mRepository->addMaterial({});
      auto mod = std::make_shared<Syx::Model>(Syx::ModelType::Cube);
      auto space = system.mSystem->createSpace();
      auto obj = system.mSystem->addPhysicsObject(false, true, space->_getHandle(), *mat, mod);

      system.mSystem->removePhysicsObject(space->_getHandle(), obj);
      mat.reset();

      Assert::AreEqual(system.mRepository->garbageCollect(), size_t(1), L"Material should be deleted because it has no handle and is not in use by physics objects", LINE_INFO());
    }

    TEST_METHOD(PhysicsSystemWithUnusedMaterial_Update_MaterialDeleted) {
      PhysicsSystemWithMaterials system;
      system.mRepository->addMaterial({});

      system.mSystem->update(1.f);

      Assert::AreEqual(system.mRepository->garbageCollect(), size_t(0), L"Material should have already been removed by the physics system update", LINE_INFO());
    }
  };
}