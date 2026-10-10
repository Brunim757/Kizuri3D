#pragma once
#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Scene.h"
#include <string>
namespace Kizuri {
struct LightmapBakeOptions {
  int resolution = 128;
  TexFormat format = TexFormat::Bc7;
  float denoise = 0.0f;
};
bool BakeLightmapFor(Scene& scene, AssetDatabase& assets, EntityId target, int resolution, std::string& texPathOut, std::string& texGuidOut, std::string& errorOut);
bool BakeLightmapForEx(Scene& scene, AssetDatabase& assets, EntityId target, const LightmapBakeOptions& options, std::string& texPathOut, std::string& texGuidOut, std::string& errorOut);
void DenoiseLightmap(float* rgb, int res, float strength);
}
