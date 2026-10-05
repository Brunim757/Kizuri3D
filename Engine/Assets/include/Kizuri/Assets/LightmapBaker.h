#pragma once
#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Scene.h"
#include <string>
namespace Kizuri {
bool BakeLightmapFor(Scene& scene, AssetDatabase& assets, EntityId target, int resolution, std::string& texPathOut, std::string& texGuidOut, std::string& errorOut);
}
