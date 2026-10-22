#pragma once
#include "Kizuri/Scene.h"
#include <string>
namespace Kizuri {
void RegisterSceneComponentSerializers();
class LogStore;
bool SaveSceneToFile(const Scene& scene, const std::string& path);
bool LoadSceneFromFile(Scene& scene, const std::string& path, LogStore* log);
}
