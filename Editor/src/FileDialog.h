#pragma once
#include <string>
namespace Kizuri {
bool ShowOpenSceneDialog(void* hwnd, std::string& outPath);
bool ShowSaveSceneDialog(void* hwnd, std::string& outPath);
bool ShowOpenGltfDialog(void* hwnd, std::string& outPath);
bool ShowOpenCubeDialog(void* hwnd, std::string& outPath);
bool ShowOpenHdrDialog(void* hwnd, std::string& outPath);
unsigned long GetLastDialogError();
}
