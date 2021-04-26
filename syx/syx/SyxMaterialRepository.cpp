#include "Precompile.h"
#include "SyxMaterialRepository.h"

namespace Syx {
  struct MaterialRepository : public IMaterialRepository {
    std::unique_ptr<IMaterialHandle> addMaterial(const Material& newMaterial) override {
      auto result = std::make_unique<MaterialHandle>();
      mMaterials.push_back(std::make_unique<OwnedMaterial>(newMaterial, *result));
      return result;
    }

    size_t garbageCollect() override {
      const size_t prev = mMaterials.size();
      OwnedMaterial::performDeferredDeletions(mMaterials);


      return prev - mMaterials.size();
    }

    std::vector<std::unique_ptr<OwnedMaterial>> mMaterials;
  };

  namespace Create {
    std::unique_ptr<IMaterialRepository> defaultMaterialRepository() {
      return std::make_unique<MaterialRepository>();
    }
  }
}