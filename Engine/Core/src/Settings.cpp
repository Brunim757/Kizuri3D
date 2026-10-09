#include "Kizuri/Settings.h"
#include <cstdio>
namespace Kizuri {
Settings::Settings() {
}
bool Settings::Load(const std::string& path) {
  values.clear();
  FILE* fp = nullptr;
  if (fopen_s(&fp, path.c_str(), "rb") != 0 || fp == nullptr) {
    return false;
  }
  std::string key;
  std::string value;
  bool inValue = false;
  bool escape = false;
  int c = 0;
  auto flush = [&]() {
    if (!key.empty()) {
      values[key] = value;
    }
    key.clear();
    value.clear();
    inValue = false;
    escape = false;
  };
  while ((c = std::fgetc(fp)) != EOF) {
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      flush();
      continue;
    }
    if (!inValue) {
      if (c == '#' && key.empty()) {
        int cc = 0;
        while ((cc = std::fgetc(fp)) != EOF && cc != '\n') {
        }
        continue;
      }
      if (c == '=') {
        inValue = true;
        continue;
      }
      if (c == ' ' || c == '\t') {
        continue;
      }
      key.push_back(static_cast<char>(c));
      continue;
    }
    if (escape) {
      if (c == 'n') {
        value.push_back('\n');
      } else {
        value.push_back(static_cast<char>(c));
      }
      escape = false;
      continue;
    }
    if (c == '\\') {
      escape = true;
      continue;
    }
    value.push_back(static_cast<char>(c));
  }
  flush();
  std::fclose(fp);
  return true;
}
bool Settings::Save(const std::string& path) const {
  FILE* fp = nullptr;
  if (fopen_s(&fp, path.c_str(), "wb") != 0 || fp == nullptr) {
    return false;
  }
  for (std::map<std::string, std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
    std::string esc;
    for (size_t i = 0; i < it->second.size(); ++i) {
      char ch = it->second[i];
      if (ch == '\\') {
        esc += "\\\\";
      } else if (ch == '\n') {
        esc += "\\n";
      } else {
        esc += ch;
      }
    }
    std::fprintf(fp, "%s=%s\n", it->first.c_str(), esc.c_str());
  }
  std::fclose(fp);
  return true;
}
void Settings::Set(const std::string& key, const std::string& value) {
  if (!key.empty()) {
    values[key] = value;
  }
}
void Settings::SetInt(const std::string& key, long long value) {
  char buf[32];
  _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%lld", value);
  Set(key, buf);
}
void Settings::SetFloat(const std::string& key, double value) {
  char buf[32];
  _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.9g", value);
  Set(key, buf);
}
void Settings::SetBool(const std::string& key, bool value) {
  Set(key, value ? "1" : "0");
}
std::string Settings::Get(const std::string& key, const std::string& fallback) const {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end()) {
    return fallback;
  }
  return it->second;
}
long long Settings::GetInt(const std::string& key, long long fallback) const {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end() || it->second.empty()) {
    return fallback;
  }
  return std::atoll(it->second.c_str());
}
double Settings::GetFloat(const std::string& key, double fallback) const {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end() || it->second.empty()) {
    return fallback;
  }
  return std::atof(it->second.c_str());
}
bool Settings::GetBool(const std::string& key, bool fallback) const {
  std::map<std::string, std::string>::const_iterator it = values.find(key);
  if (it == values.end() || it->second.empty()) {
    return fallback;
  }
  const std::string& v = it->second;
  if (v == "1" || v == "true" || v == "yes" || v == "on") {
    return true;
  }
  if (v == "0" || v == "false" || v == "no" || v == "off") {
    return false;
  }
  return fallback;
}
bool Settings::Has(const std::string& key) const {
  return values.find(key) != values.end();
}
bool Settings::Remove(const std::string& key) {
  return values.erase(key) > 0;
}
void Settings::Clear() {
  values.clear();
}
std::vector<std::string> Settings::Keys() const {
  std::vector<std::string> out;
  for (std::map<std::string, std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
    out.push_back(it->first);
  }
  return out;
}
}
