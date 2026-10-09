#pragma once
#include <map>
#include <string>
#include <vector>
namespace Kizuri {
class Settings {
public:
  Settings();
  bool Load(const std::string& path);
  bool Save(const std::string& path) const;
  void Set(const std::string& key, const std::string& value);
  void SetInt(const std::string& key, long long value);
  void SetFloat(const std::string& key, double value);
  void SetBool(const std::string& key, bool value);
  std::string Get(const std::string& key, const std::string& fallback) const;
  long long GetInt(const std::string& key, long long fallback) const;
  double GetFloat(const std::string& key, double fallback) const;
  bool GetBool(const std::string& key, bool fallback) const;
  bool Has(const std::string& key) const;
  bool Remove(const std::string& key);
  void Clear();
  std::vector<std::string> Keys() const;
private:
  std::map<std::string, std::string> values;
};
}
