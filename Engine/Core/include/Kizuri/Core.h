#pragma once
namespace Kizuri {
const char* BuildId();
const char* Core_Version();
bool Core_TestDirectXMath();
bool Core_TestTaskSystem();
bool Core_TestZstd();
bool Core_TestFlatBuffers();
}
