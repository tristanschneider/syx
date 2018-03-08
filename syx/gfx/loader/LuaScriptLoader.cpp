#include "Precompile.h"
#include "loader/LuaScriptLoader.h"

#include "asset/LuaScript.h"
#include "system/AssetRepo.h"

RegisterAssetLoader("lc", TextAssetLoader, TextAsset);
