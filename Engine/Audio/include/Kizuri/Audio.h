#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
namespace Kizuri {
enum class AudioRolloffModel : int {
  Linear = 0,
  Inverse = 1,
  Exponential = 2
};
struct AudioClip {
  std::vector<float> samples;
  uint32_t channels = 2;
  uint32_t sampleRate = 48000;
  uint64_t frameCount = 0;
  float Duration() const {
    return sampleRate > 0 ? static_cast<float>(frameCount) / static_cast<float>(sampleRate) : 0.0f;
  }
};
bool LoadAudioClipFromFile(const std::string& path, uint32_t targetSampleRate, AudioClip& out);
struct AudioDuckLink {
  int targetBus = -1;
  float amountDb = -6.0f;
};
struct AudioBusDesc {
  std::string name = "Bus";
  int parent = -1;
  float volume = 1.0f;
  bool muted = false;
  bool soloed = false;
  bool reverbEnabled = false;
  float reverbRoomSize = 0.6f;
  float reverbDamping = 0.4f;
  float reverbWidth = 1.0f;
  float reverbWet = 0.25f;
  std::vector<AudioDuckLink> ducking;
};
class AudioReverb {
public:
  static constexpr int kCombs = 8;
  static constexpr int kAllpass = 4;
  static constexpr int kMaxChannels = 2;
  struct Comb {
    std::vector<float> buffer;
    int index = 0;
    float feedback = 0.84f;
    float store = 0.0f;
    int size = 1116;
  };
  struct Allpass {
    std::vector<float> buffer;
    int index = 0;
    float feedback = 0.5f;
    int size = 556;
  };
  AudioReverb();
  void Configure(uint32_t sampleRate, float roomSize, float damping, float width);
  void Process(const float* input, float* output, uint32_t frames, uint32_t channels, float wetMix);
  void Reset();
private:
  uint32_t sampleRate = 48000;
  float roomSize = 0.6f;
  float damping = 0.4f;
  float width = 1.0f;
  Comb combs[kMaxChannels][kCombs];
  Allpass allpass[kMaxChannels][kAllpass];
  float preDelayL = 0.0f;
  float preDelayR = 0.0f;
};
class AudioMixer {
public:
  AudioMixer();
  void Reset();
  uint32_t AddBus(const AudioBusDesc& desc);
  size_t BusCount() const { return buses.size(); }
  const AudioBusDesc* GetBus(uint32_t index) const;
  AudioBusDesc* GetBusMutable(uint32_t index);
  uint32_t MasterBus() const { return 0; }
  int FindBusByName(const std::string& name) const;
  float ResolveBusGain(uint32_t index) const;
  float ResolveChainGain(uint32_t index) const;
  int BusDepth(uint32_t index) const;
  bool AnySolo() const;
  bool IsSoloChain(uint32_t index) const;
  void ComputeOrder(std::vector<uint32_t>& outOrder) const;
  bool SaveToFile(const std::string& path) const;
  bool LoadFromFile(const std::string& path);
private:
  std::vector<AudioBusDesc> buses;
};
class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();
  bool Initialize(uint32_t sampleRate = 48000, uint32_t channels = 2, bool startDevice = true);
  void Shutdown();
  bool IsInitialized() const { return initialized; }
  bool HasDevice() const { return deviceReady; }
  uint32_t SampleRate() const { return sampleRate; }
  uint32_t ChannelCount() const { return channels; }
  AudioMixer& Mixer() { return mixer; }
  const AudioMixer& Mixer() const { return mixer; }
  uint32_t AddClip(const AudioClip& clip);
  bool LoadClip(const std::string& path, uint32_t& outClipId);
  size_t ClipCount() const { return clips.size(); }
  uint32_t AddSource(uint32_t clipId, uint32_t busId);
  bool RemoveSource(uint32_t sourceId);
  bool SetSourcePlaying(uint32_t sourceId, bool playing);
  bool SetSourceGain(uint32_t sourceId, float gain);
  bool SetSourcePosition(uint32_t sourceId, const float pos[3]);
  bool SetSourceBus(uint32_t sourceId, uint32_t busId);
  bool SetSourceSpatial(uint32_t sourceId, bool spatial);
  bool SetSourceLoop(uint32_t sourceId, bool loop);
  bool SetSourceRolloff(uint32_t sourceId, AudioRolloffModel model, float rolloff);
  void SetListener(const float pos[3], const float forward[3], const float up[3]);
  const float* ListenerPosition() const { return listenerPos; }
  void Update();
  void Render(float* outInterleaved, uint32_t frames);
  uint32_t ActiveVoiceCount() const;
  float MasterPeak() const { return masterPeak; }
  static float ComputeAttenuation(float distance, float minDistance, float maxDistance, AudioRolloffModel model, float rolloff);
private:
  struct VoiceCmd {
    enum class Kind : int { Add = 0, Remove = 1, Update = 2 };
    Kind kind = Kind::Add;
    uint32_t sourceId = 0;
    uint32_t clipId = 0xFFFFFFFFu;
    uint32_t busId = 0;
    float gain = 1.0f;
    bool playing = true;
    bool spatial = true;
    bool loop = false;
    AudioRolloffModel rolloff = AudioRolloffModel::Inverse;
    float rolloffFactor = 1.0f;
    float position[3] = { 0.0f, 0.0f, 0.0f };
  };
  struct Voice {
    uint32_t sourceId = 0;
    std::shared_ptr<AudioClip> clip;
    uint32_t busId = 0;
    float gain = 1.0f;
    bool playing = true;
    bool spatial = true;
    bool loop = false;
    AudioRolloffModel rolloff = AudioRolloffModel::Inverse;
    float rolloffFactor = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 20.0f;
    float position[3] = { 0.0f, 0.0f, 0.0f };
    uint64_t cursor = 0;
  };
  struct BusMix {
    int parent = -1;
    float volume = 1.0f;
    bool muted = false;
    bool soloed = false;
    bool reverbEnabled = false;
    float reverbRoomSize = 0.6f;
    float reverbDamping = 0.4f;
    float reverbWidth = 1.0f;
    float reverbWet = 0.25f;
    std::vector<AudioDuckLink> ducking;
  };
  struct MixSnapshot {
    std::vector<BusMix> buses;
    std::vector<uint32_t> order;
  };
  void ApplyCommands();
  void EnsureBuffers(size_t busCount, uint32_t frames);
  float ResolveLiveGain(uint32_t index) const;
  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;
  AudioMixer mixer;
  std::vector<std::shared_ptr<AudioClip>> clips;
  std::mutex commandMutex;
  std::vector<VoiceCmd> commands;
  std::mutex mixMutex;
  MixSnapshot pendingMix;
  MixSnapshot liveMix;
  std::vector<Voice> voices;
  std::vector<std::vector<float>> busBuffers;
  std::vector<std::unique_ptr<AudioReverb>> reverbs;
  std::vector<float> busActivity;
  std::vector<float> duckFactors;
  std::vector<float> scratch;
  std::mutex renderMutex;
  std::atomic<uint32_t> activeVoices{ 0 };
  uint32_t nextSourceId = 1;
  float listenerPos[3] = { 0.0f, 0.0f, 0.0f };
  float listenerForward[3] = { 0.0f, 0.0f, 1.0f };
  float listenerUp[3] = { 0.0f, 1.0f, 0.0f };
  uint32_t sampleRate = 48000;
  uint32_t channels = 2;
  bool initialized = false;
  bool deviceReady = false;
  float masterPeak = 0.0f;
  void* device = nullptr;
  void* context = nullptr;
};
}