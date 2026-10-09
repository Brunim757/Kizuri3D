#include "Kizuri/Path.h"
#include <cctype>
namespace Kizuri {
namespace Path {
std::string FileName(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos) {
    return path;
  }
  return path.substr(slash + 1);
}
std::string Stem(const std::string& path) {
  std::string fn = FileName(path);
  size_t dot = fn.find_last_of('.');
  if (dot == std::string::npos || dot == 0) {
    return fn;
  }
  return fn.substr(0, dot);
}
std::string Extension(const std::string& path) {
  std::string fn = FileName(path);
  size_t dot = fn.find_last_of('.');
  if (dot == std::string::npos || dot == 0) {
    return "";
  }
  return fn.substr(dot);
}
std::string ExtensionLower(const std::string& path) {
  std::string ext = Extension(path);
  for (size_t i = 0; i < ext.size(); ++i) {
    if (ext[i] >= 'A' && ext[i] <= 'Z') {
      ext[i] = static_cast<char>(ext[i] + ('a' - 'A'));
    }
  }
  return ext;
}
std::string Parent(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos) {
    return "";
  }
  if (slash == 0) {
    return path.substr(0, 1);
  }
  return path.substr(0, slash);
}
std::string Combine(const std::string& dir, const std::string& name) {
  if (dir.empty()) {
    return name;
  }
  if (name.empty()) {
    return dir;
  }
  char last = dir[dir.size() - 1];
  if (last == '/' || last == '\\') {
    return dir + name;
  }
  return dir + "/" + name;
}
std::string Normalize(const std::string& path) {
  std::string out = path;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == '\\') {
      out[i] = '/';
    }
  }
  return out;
}
bool HasExtension(const std::string& path, const char* ext) {
  if (ext == nullptr) {
    return false;
  }
  return ExtensionLower(path) == ext;
}
}
namespace Str {
void ToLowerInPlace(std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    s[i] = static_cast<char>(std::tolower(c));
  }
}
std::string ToLowerCopy(const std::string& s) {
  std::string out = s;
  ToLowerInPlace(out);
  return out;
}
bool StartsWith(const std::string& s, const std::string& prefix) {
  if (prefix.size() > s.size()) {
    return false;
  }
  return s.compare(0, prefix.size(), prefix) == 0;
}
bool EndsWith(const std::string& s, const std::string& suffix) {
  if (suffix.size() > s.size()) {
    return false;
  }
  return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
std::string Trim(const std::string& s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0) {
    ++b;
  }
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) {
    --e;
  }
  return s.substr(b, e - b);
}
}
}
