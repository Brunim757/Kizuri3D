#include "Kizuri/RenderGraph.h"
namespace Kizuri {
int RenderGraph::AddResource(const std::string& name, int w, int h, int format) {
  RGResourceDesc desc;
  desc.name = name;
  desc.w = w < 1 ? 1 : w;
  desc.h = h < 1 ? 1 : h;
  desc.format = format;
  desc.external = false;
  resources.push_back(desc);
  compiled = false;
  return static_cast<int>(resources.size() - 1);
}
int RenderGraph::AddExternalResource(const std::string& name) {
  RGResourceDesc desc;
  desc.name = name;
  desc.external = true;
  resources.push_back(desc);
  compiled = false;
  return static_cast<int>(resources.size() - 1);
}
int RenderGraph::AddPass(const std::string& name, const std::vector<int>& reads, const std::vector<int>& writes, const std::function<void()>& execute) {
  RGPassDesc desc;
  desc.name = name;
  desc.reads = reads;
  desc.writes = writes;
  desc.execute = execute;
  passes.push_back(desc);
  compiled = false;
  return static_cast<int>(passes.size() - 1);
}
bool RenderGraph::Compile() {
  order.clear();
  backing.assign(resources.size(), -1);
  if (passes.empty()) {
    compiled = true;
    return true;
  }
  std::vector<int> writerOf(resources.size(), -1);
  for (size_t p = 0; p < passes.size(); ++p) {
    for (size_t i = 0; i < passes[p].writes.size(); ++i) {
      int r = passes[p].writes[i];
      if (r < 0 || r >= static_cast<int>(resources.size())) {
        return false;
      }
      if (writerOf[r] >= 0) {
        return false;
      }
      writerOf[r] = static_cast<int>(p);
    }
    for (size_t i = 0; i < passes[p].reads.size(); ++i) {
      int r = passes[p].reads[i];
      if (r < 0 || r >= static_cast<int>(resources.size())) {
        return false;
      }
    }
  }
  std::vector<std::vector<int>> deps(passes.size());
  std::vector<int> indegree(passes.size(), 0);
  for (size_t p = 0; p < passes.size(); ++p) {
    for (size_t i = 0; i < passes[p].reads.size(); ++i) {
      int w = writerOf[passes[p].reads[i]];
      if (w >= 0 && w != static_cast<int>(p)) {
        bool known = false;
        for (size_t d = 0; d < deps[w].size(); ++d) {
          if (deps[w][d] == static_cast<int>(p)) {
            known = true;
            break;
          }
        }
        if (!known) {
          deps[w].push_back(static_cast<int>(p));
          ++indegree[p];
        }
      }
    }
  }
  std::vector<int> queue;
  for (size_t p = 0; p < passes.size(); ++p) {
    if (indegree[p] == 0) {
      queue.push_back(static_cast<int>(p));
    }
  }
  while (!queue.empty()) {
    int cur = queue.back();
    queue.pop_back();
    order.push_back(cur);
    for (size_t d = 0; d < deps[cur].size(); ++d) {
      if (--indegree[deps[cur][d]] == 0) {
        queue.push_back(deps[cur][d]);
      }
    }
  }
  if (order.size() != passes.size()) {
    order.clear();
    return false;
  }
  std::vector<int> firstUse(resources.size(), -1);
  std::vector<int> lastUse(resources.size(), -1);  for (size_t o = 0; o < order.size(); ++o) {
    int p = order[o];
    for (size_t i = 0; i < passes[p].reads.size(); ++i) {
      int r = passes[p].reads[i];
      if (firstUse[r] < 0) {
        firstUse[r] = static_cast<int>(o);
      }
      lastUse[r] = static_cast<int>(o);
    }
    for (size_t i = 0; i < passes[p].writes.size(); ++i) {
      int r = passes[p].writes[i];
      if (firstUse[r] < 0) {
        firstUse[r] = static_cast<int>(o);
      }
      lastUse[r] = static_cast<int>(o);
    }
  }
  struct LiveRange {
    int resource;
    int first;
    int last;
  };
  std::vector<LiveRange> ranges;
  for (size_t r = 0; r < resources.size(); ++r) {
    if (firstUse[r] >= 0 && !resources[r].external) {
      LiveRange lr;
      lr.resource = static_cast<int>(r);
      lr.first = firstUse[r];
      lr.last = lastUse[r];
      ranges.push_back(lr);
    }
  }
  int nextBacking = 0;
  for (size_t i = 0; i < ranges.size(); ++i) {
    int assigned = -1;
    for (size_t j = 0; j < i; ++j) {
      bool overlap = !(ranges[i].last < ranges[j].first || ranges[j].last < ranges[i].first);
      bool sameSize = resources[ranges[i].resource].w == resources[ranges[j].resource].w && resources[ranges[i].resource].h == resources[ranges[j].resource].h && resources[ranges[i].resource].format == resources[ranges[j].resource].format;
      if (!overlap && sameSize && backing[ranges[j].resource] >= 0) {
        bool stolen = false;
        for (size_t k = 0; k < i; ++k) {
          if (k != j && backing[ranges[k].resource] == backing[ranges[j].resource]) {
            bool overlapK = !(ranges[i].last < ranges[k].first || ranges[k].last < ranges[i].first);
            if (overlapK) {
              stolen = true;
              break;
            }
          }
        }
        if (!stolen) {
          assigned = backing[ranges[j].resource];
          break;
        }
      }
    }
    if (assigned < 0) {
      assigned = nextBacking++;
    }
    backing[ranges[i].resource] = assigned;
  }
  compiled = true;
  return true;
}
void RenderGraph::Execute() {
  if (!compiled) {
    return;
  }
  for (size_t o = 0; o < order.size(); ++o) {
    const std::function<void()>& fn = passes[order[o]].execute;
    if (fn) {
      fn();
    }
  }
}
size_t RenderGraph::PassCount() const {
  return passes.size();
}
size_t RenderGraph::ResourceCount() const {
  return resources.size();
}
int RenderGraph::PassOrderIndex(size_t orderPos) const {
  if (orderPos >= order.size()) {
    return -1;
  }
  return order[orderPos];
}
int RenderGraph::BackingOf(int resource) const {
  if (resource < 0 || resource >= static_cast<int>(backing.size())) {
    return -1;
  }
  return backing[resource];
}
size_t RenderGraph::TransientCount() const {
  size_t n = 0;
  for (size_t i = 0; i < backing.size(); ++i) {
    if (backing[i] >= 0) {
      ++n;
    }
  }
  return n;
}
}
