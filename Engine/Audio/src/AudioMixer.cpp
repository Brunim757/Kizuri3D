#include "Kizuri/Audio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
namespace Kizuri {
static const int kCombTuning[AudioReverb::kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int kAllpassTuning[AudioReverb::kAllpass] = { 556, 441, 341, 225 };
static const int kStereoSpread = 23;
AudioReverb::AudioReverb() {
  Reset();
}
void AudioReverb::Reset() {
  for (int c = 0; c < kMaxChannels; ++c) {
    for (int i = 0; i < kCombs; ++i) {
      combs[c][i].buffer.assign(4096, 0.0f);
      combs[c][i].index = 0;
      combs[c][i].store = 0.0f;
    }
    for (int i = 0; i < kAllpass; ++i) {
      allpass[c][i].buffer.assign(4096, 0.0f);
      allpass[c][i].index = 0;
    }
  }
  preDelayL = 0.0f;
  preDelayR = 0.0f;
}
void AudioReverb::Configure(uint32_t inSampleRate, float inRoomSize, float inDamping, float inWidth) {
  sampleRate = inSampleRate > 0 ? inSampleRate : 48000;
  roomSize = std::min(std::max(inRoomSize, 0.0f), 1.0f);
  damping = std::min(std::max(inDamping, 0.0f), 1.0f);
  width = std::min(std::max(inWidth, 0.0f), 1.0f);
  const float scale = static_cast<float>(sampleRate) / 44100.0f;
  for (int c = 0; c < kMaxChannels; ++c) {
    const int spread = c == 1 ? kStereoSpread : 0;
    for (int i = 0; i < kCombs; ++i) {
      int size = static_cast<int>(static_cast<float>(kCombTuning[i] + spread) * scale);
      combs[c][i].size = std::min(std::max(size, 8), 4095);
      combs[c][i].feedback = 0.7f + roomSize * 0.28f;
      if (static_cast<int>(combs[c][i].buffer.size()) < combs[c][i].size) {
        combs[c][i].buffer.assign(static_cast<size_t>(combs[c][i].size), 0.0f);
      }
    }
    for (int i = 0; i < kAllpass; ++i) {
      int size = static_cast<int>(static_cast<float>(kAllpassTuning[i] + spread) * scale);
      allpass[c][i].size = std::min(std::max(size, 8), 4095);
      allpass[c][i].feedback = 0.5f;
      if (static_cast<int>(allpass[c][i].buffer.size()) < allpass[c][i].size) {
        allpass[c][i].buffer.assign(static_cast<size_t>(allpass[c][i].size), 0.0f);
      }
    }
  }
}
static void ReverbProcessChannel(AudioReverb::Comb* combs, AudioReverb::Allpass* allpass, float* samples, uint32_t frames, float damping) {
  const float damp = 0.2f + damping * 0.8f;
  for (uint32_t f = 0; f < frames; ++f) {
    const float input = samples[f];
    float out = 0.0f;
    for (int i = 0; i < AudioReverb::kCombs; ++i) {
      AudioReverb::Comb& c = combs[i];
      const float y = c.buffer[c.index];
      c.store = y * (1.0f - damp) + c.store * damp;
      c.buffer[c.index] = input + c.store * c.feedback;
      c.index = (c.index + 1) % c.size;
      out += y;
    }
    for (int i = 0; i < AudioReverb::kAllpass; ++i) {
      AudioReverb::Allpass& a = allpass[i];
      const float bufout = a.buffer[a.index];
      const float y = -input + bufout;
      a.buffer[a.index] = input + bufout * a.feedback;
      a.index = (a.index + 1) % a.size;
      out += y;
    }
    samples[f] = out * 0.015f;
  }
}
void AudioReverb::Process(const float* input, float* output, uint32_t frames, uint32_t channels, float wetMix) {
  const uint32_t ch = channels < 1 ? 1 : (channels > kMaxChannels ? kMaxChannels : channels);
  float wetL[2048];
  float wetR[2048];
  const uint32_t block = frames < 2048 ? frames : 2048;
  for (uint32_t base = 0; base < frames; base += block) {
    const uint32_t n = std::min(block, frames - base);
    for (uint32_t c = 0; c < ch; ++c) {
      float scratch[2048];
      for (uint32_t i = 0; i < n; ++i) {
        scratch[i] = input[(base + i) * channels + c];
      }
      ReverbProcessChannel(combs[c], allpass[c], scratch, n, damping);
      if (c == 0) {
        for (uint32_t i = 0; i < n; ++i) {
          wetL[i] = scratch[i];
        }
      } else {
        for (uint32_t i = 0; i < n; ++i) {
          wetR[i] = scratch[i];
        }
      }
    }
    const float wet = std::min(std::max(wetMix, 0.0f), 1.0f);
    const float dry = 1.0f - wet * 0.5f;
    for (uint32_t i = 0; i < n; ++i) {
      const float l = wetL[i];
      const float r = (ch > 1) ? wetR[i] : l;
      const float spread = 0.5f * (1.0f + width);
      const float spread2 = 0.5f * (1.0f - width);
      output[(base + i) * channels + 0] = input[(base + i) * channels + 0] * dry + (l * spread + r * spread2) * wet;
      if (channels > 1) {
        output[(base + i) * channels + 1] = input[(base + i) * channels + 1] * dry + (l * spread2 + r * spread) * wet;
      }
    }
  }
}
AudioMixer::AudioMixer() {
  Reset();
}
void AudioMixer::Reset() {
  buses.clear();
  AudioBusDesc master;
  master.name = "Master";
  master.parent = -1;
  buses.push_back(master);
}
uint32_t AudioMixer::AddBus(const AudioBusDesc& desc) {
  AudioBusDesc copy = desc;
  if (copy.parent < 0 || static_cast<size_t>(copy.parent) >= buses.size()) {
    copy.parent = 0;
  }
  buses.push_back(copy);
  return static_cast<uint32_t>(buses.size() - 1);
}
const AudioBusDesc* AudioMixer::GetBus(uint32_t index) const {
  if (index >= buses.size()) {
    return nullptr;
  }
  return &buses[index];
}
AudioBusDesc* AudioMixer::GetBusMutable(uint32_t index) {
  if (index >= buses.size()) {
    return nullptr;
  }
  return &buses[index];
}
int AudioMixer::FindBusByName(const std::string& name) const {
  for (size_t i = 0; i < buses.size(); ++i) {
    if (buses[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
bool AudioMixer::AnySolo() const {
  for (size_t i = 1; i < buses.size(); ++i) {
    if (buses[i].soloed) {
      return true;
    }
  }
  return false;
}
bool AudioMixer::IsSoloChain(uint32_t index) const {
  uint32_t cur = index;
  int guard = 0;
  while (cur != 0xFFFFFFFFu && guard < 256) {
    if (buses[cur].soloed) {
      return true;
    }
    const int parent = buses[cur].parent;
    if (parent < 0) {
      break;
    }
    cur = static_cast<uint32_t>(parent);
    ++guard;
  }
  return false;
}
float AudioMixer::ResolveBusGain(uint32_t index) const {
  if (index >= buses.size()) {
    return 0.0f;
  }
  const AudioBusDesc& b = buses[index];
  float gain = b.volume;
  if (b.muted) {
    gain = 0.0f;
  }
  if (index != 0 && AnySolo() && !IsSoloChain(index)) {
    gain = 0.0f;
  }
  return gain;
}
float AudioMixer::ResolveChainGain(uint32_t index) const {
  float gain = 1.0f;
  uint32_t cur = index;
  int guard = 0;
  while (cur != 0xFFFFFFFFu && guard < 256) {
    gain *= ResolveBusGain(cur);
    const int parent = buses[cur].parent;
    if (parent < 0) {
      break;
    }
    cur = static_cast<uint32_t>(parent);
    ++guard;
  }
  return gain;
}
int AudioMixer::BusDepth(uint32_t index) const {
  int depth = 0;
  uint32_t cur = index;
  int guard = 0;
  while (cur != 0xFFFFFFFFu && guard < 256) {
    const int parent = buses[cur].parent;
    if (parent < 0) {
      break;
    }
    cur = static_cast<uint32_t>(parent);
    ++depth;
    ++guard;
  }
  return depth;
}
void AudioMixer::ComputeOrder(std::vector<uint32_t>& outOrder) const {
  outOrder.clear();
  outOrder.reserve(buses.size());
  for (size_t i = 0; i < buses.size(); ++i) {
    outOrder.push_back(static_cast<uint32_t>(i));
  }
  std::stable_sort(outOrder.begin(), outOrder.end(), [this](uint32_t a, uint32_t b) {
    return BusDepth(a) > BusDepth(b);
  });
}
bool AudioMixer::SaveToFile(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out << "KIZURI_MIXER 1\n";
  for (size_t i = 0; i < buses.size(); ++i) {
    const AudioBusDesc& b = buses[i];
    out << "BUS " << i << " \"" << b.name << "\" " << b.parent << " " << b.volume << " " << (b.muted ? 1 : 0) << " " << (b.soloed ? 1 : 0) << " " << (b.reverbEnabled ? 1 : 0) << " " << b.reverbRoomSize << " " << b.reverbDamping << " " << b.reverbWidth << " " << b.reverbWet << "\n";
    for (size_t d = 0; d < b.ducking.size(); ++d) {
      out << "DUCK " << i << " " << b.ducking[d].targetBus << " " << b.ducking[d].amountDb << "\n";
    }
  }
  return out.good();
}
static bool ParseQuoted(std::istringstream& in, std::string& out) {
  std::string token;
  if (!(in >> token)) {
    return false;
  }
  if (!token.empty() && token[0] == '"') {
    std::string rest = token.substr(1);
    while (rest.find('"') == std::string::npos && (in >> token)) {
      rest += " " + token;
    }
    const size_t end = rest.find('"');
    if (end == std::string::npos) {
      return false;
    }
    out = rest.substr(0, end);
    return true;
  }
  out = token;
  return true;
}
bool AudioMixer::LoadFromFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::string line;
  if (!std::getline(in, line)) {
    return false;
  }
  if (line.rfind("KIZURI_MIXER", 0) != 0) {
    return false;
  }
  std::vector<AudioBusDesc> parsed;
  std::vector<std::pair<size_t, AudioDuckLink>> ducks;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream ls(line);
    std::string kind;
    ls >> kind;
    if (kind == "BUS") {
      size_t index = 0;
      int parent = -1;
      int muted = 0;
      int soloed = 0;
      int reverb = 0;
      AudioBusDesc b;
      ls >> index;
      if (!ParseQuoted(ls, b.name)) {
        return false;
      }
      ls >> parent;
      ls >> b.volume;
      ls >> muted;
      ls >> soloed;
      ls >> reverb;
      ls >> b.reverbRoomSize;
      ls >> b.reverbDamping;
      ls >> b.reverbWidth;
      ls >> b.reverbWet;
      b.parent = parent;
      b.muted = muted != 0;
      b.soloed = soloed != 0;
      b.reverbEnabled = reverb != 0;
      if (parsed.size() <= index) {
        parsed.resize(index + 1);
      }
      parsed[index] = b;
    } else if (kind == "DUCK") {
      size_t index = 0;
      int target = -1;
      float amountDb = -6.0f;
      ls >> index >> target >> amountDb;
      AudioDuckLink link;
      link.targetBus = target;
      link.amountDb = amountDb;
      ducks.push_back(std::make_pair(index, link));
    }
  }
  if (parsed.empty()) {
    return false;
  }
  for (size_t i = 0; i < ducks.size(); ++i) {
    if (ducks[i].first < parsed.size()) {
      parsed[ducks[i].first].ducking.push_back(ducks[i].second);
    }
  }
  buses = parsed;
  for (size_t i = 0; i < buses.size(); ++i) {
    if (buses[i].parent < 0 || static_cast<size_t>(buses[i].parent) >= buses.size()) {
      buses[i].parent = i == 0 ? -1 : 0;
    }
  }
  return true;
}
}