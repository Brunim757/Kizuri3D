#pragma once
#include <string>
namespace Kizuri {
namespace Path {
std::string FileName(const std::string& path);
std::string Stem(const std::string& path);
std::string Extension(const std::string& path);
std::string ExtensionLower(const std::string& path);
std::string Parent(const std::string& path);
std::string Combine(const std::string& dir, const std::string& name);
std::string Normalize(const std::string& path);
bool HasExtension(const std::string& path, const char* ext);
}
namespace Str {
void ToLowerInPlace(std::string& s);
std::string ToLowerCopy(const std::string& s);
bool StartsWith(const std::string& s, const std::string& prefix);
bool EndsWith(const std::string& s, const std::string& suffix);
std::string Trim(const std::string& s);
}
}
