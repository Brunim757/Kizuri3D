#pragma once
#include <functional>
#include <string>
#include <vector>
namespace Kizuri {
struct RGResourceDesc {
  std::string name;
  int w = 1;
  int h = 1;
  int format = 0;
  bool external = false;
};
struct RGPassDesc {
  std::string name;
  std::vector<int> reads;
  std::vector<int> writes;
  std::function<void()> execute;
};
class RenderGraph {
public:
  int AddResource(const std::string& name, int w, int h, int format);
  int AddExternalResource(const std::string& name);
  int AddPass(const std::string& name, const std::vector<int>& reads, const std::vector<int>& writes, const std::function<void()>& execute);
  bool Compile();
  void Execute();
  size_t PassCount() const;
  size_t ResourceCount() const;
  int PassOrderIndex(size_t orderPos) const;
  int BackingOf(int resource) const;
  size_t TransientCount() const;
private:
  std::vector<RGResourceDesc> resources;
  std::vector<RGPassDesc> passes;
  std::vector<int> order;
  std::vector<int> backing;
  bool compiled = false;
};
}
