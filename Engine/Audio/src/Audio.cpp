#include "Kizuri/Audio.h"
#include <miniaudio.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace Kizuri {
const char* Audio_Version() {
  return "0.1.0-fase7";
}
bool Audio_TestEngineNoDevice() {
  ma_engine_config cfg = ma_engine_config_init();
  cfg.noDevice = MA_TRUE;
  cfg.channels = 2;
  cfg.sampleRate = 48000;
  ma_engine engine;
  ma_result r = ma_engine_init(&cfg, &engine);
  if (r != MA_SUCCESS) {
    return false;
  }
  ma_engine_uninit(&engine);
  return true;
}
bool LoadAudioClipFromFile(const std::string& path, uint32_t targetSampleRate, AudioClip& out) {
  if (path.empty()) {
    return false;
  }
  const uint32_t rate = targetSampleRate > 0 ? targetSampleRate : 48000;
  ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, rate);
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
    return false;
  }
  ma_uint64 totalFrames = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS || totalFrames == 0) {
    ma_decoder_uninit(&decoder);
    return false;
  }
  out.samples.assign(static_cast<size_t>(totalFrames) * 2, 0.0f);
  ma_uint64 framesRead = 0;
  ma_result result = ma_decoder_read_pcm_frames(&decoder, out.samples.data(), totalFrames, &framesRead);
  ma_decoder_uninit(&decoder);
  if (result != MA_SUCCESS && result != MA_AT_END) {
    return false;
  }
  out.channels = 2;
  out.sampleRate = rate;
  out.frameCount = static_cast<uint64_t>(framesRead);
  out.samples.resize(static_cast<size_t>(out.frameCount) * 2);
  return out.frameCount > 0;
}
static void AudioDeviceCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
  (void)pInput;
  if (pDevice == nullptr || pOutput == nullptr) {
    return;
  }
  AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
  if (engine != nullptr) {
    engine->Render(static_cast<float*>(pOutput), frameCount);
  }
}
AudioEngine::AudioEngine() {
}
AudioEngine::~AudioEngine() {
  Shutdown();
}
bool AudioEngine::Initialize(uint32_t inSampleRate, uint32_t inChannels, bool startDevice) {
  if (initialized) {
    return true;
  }
  sampleRate = inSampleRate > 0 ? inSampleRate : 48000;
  channels = inChannels > 0 ? (inChannels > 2 ? 2 : inChannels) : 2;
  mixer.Reset();
  const char* defaults[4] = { "Music", "SFX", "Voice", "Ambience" };
  for (int i = 0; i < 4; ++i) {
    AudioBusDesc desc;
    desc.name = defaults[i];
    desc.parent = 0;
    mixer.AddBus(desc);
  }
  AudioBusDesc voice;
  const int voiceIndex = mixer.FindBusByName("Voice");
  if (voiceIndex >= 0) {
    voice = *mixer.GetBusMutable(static_cast<uint32_t>(voiceIndex));
    AudioDuckLink link;
    link.targetBus = mixer.FindBusByName("Music");
    link.amountDb = -8.0f;
    voice.ducking.push_back(link);
    AudioDuckLink link2;
    link2.targetBus = mixer.FindBusByName("Ambience");
    link2.amountDb = -6.0f;
    voice.ducking.push_back(link2);
    *mixer.GetBusMutable(static_cast<uint32_t>(voiceIndex)) = voice;
  }
  pendingMix.buses.clear();
  for (size_t i = 0; i < mixer.BusCount(); ++i) {
    const AudioBusDesc* b = mixer.GetBus(static_cast<uint32_t>(i));
    BusMix bm;
    bm.parent = b->parent;
    bm.volume = b->volume;
    bm.muted = b->muted;
    bm.soloed = b->soloed;
    bm.reverbEnabled = b->reverbEnabled;
    bm.reverbRoomSize = b->reverbRoomSize;
    bm.reverbDamping = b->reverbDamping;
    bm.reverbWidth = b->reverbWidth;
    bm.reverbWet = b->reverbWet;
    bm.ducking = b->ducking;
    pendingMix.buses.push_back(bm);
  }
  mixer.ComputeOrder(pendingMix.order);
  ma_device* dev = new ma_device();
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = channels;
  config.sampleRate = sampleRate;
  config.dataCallback = &AudioDeviceCallback;
  config.pUserData = this;
  ma_result result = ma_device_init(nullptr, &config, dev);
  if (result != MA_SUCCESS) {
    delete dev;
    device = nullptr;
    deviceReady = false;
  } else {
    device = dev;
    deviceReady = true;
    if (startDevice) {
      ma_device_start(dev);
    }
  }
  initialized = true;
  return true;
}
void AudioEngine::Shutdown() {
  if (!initialized) {
    return;
  }
  initialized = false;
  if (device != nullptr) {
    ma_device* dev = static_cast<ma_device*>(device);
    ma_device_stop(dev);
    ma_device_uninit(dev);
    delete dev;
    device = nullptr;
  }
  deviceReady = false;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.clear();
  }
  voices.clear();
}
uint32_t AudioEngine::AddClip(const AudioClip& clip) {
  if (clip.frameCount == 0 || clip.samples.empty()) {
    return 0xFFFFFFFFu;
  }
  std::shared_ptr<AudioClip> shared = std::make_shared<AudioClip>(clip);
  clips.push_back(shared);
  return static_cast<uint32_t>(clips.size() - 1);
}
bool AudioEngine::LoadClip(const std::string& path, uint32_t& outClipId) {
  AudioClip clip;
  if (!LoadAudioClipFromFile(path, sampleRate, clip)) {
    return false;
  }
  const uint32_t id = AddClip(clip);
  if (id == 0xFFFFFFFFu) {
    return false;
  }
  outClipId = id;
  return true;
}
uint32_t AudioEngine::AddSource(uint32_t clipId, uint32_t busId) {
  if (clipId >= clips.size()) {
    return 0;
  }
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Add;
  cmd.sourceId = nextSourceId++;
  cmd.clipId = clipId;
  cmd.busId = busId;
  cmd.gain = 1.0f;
  cmd.playing = true;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return cmd.sourceId;
}
bool AudioEngine::RemoveSource(uint32_t sourceId) {
  if (sourceId == 0) {
    return false;
  }
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Remove;
  cmd.sourceId = sourceId;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourcePlaying(uint32_t sourceId, bool playing) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldPlaying;
  cmd.playing = playing;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourceGain(uint32_t sourceId, float gain) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldGain;
  cmd.gain = gain;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourcePosition(uint32_t sourceId, const float pos[3]) {
  if (pos == nullptr) {
    return false;
  }
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldPosition;
  cmd.position[0] = pos[0];
  cmd.position[1] = pos[1];
  cmd.position[2] = pos[2];
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourceBus(uint32_t sourceId, uint32_t busId) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldBus;
  cmd.busId = busId;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourceSpatial(uint32_t sourceId, bool spatial) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldSpatial;
  cmd.spatial = spatial;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourceLoop(uint32_t sourceId, bool loop) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldLoop;
  cmd.loop = loop;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
bool AudioEngine::SetSourceRolloff(uint32_t sourceId, AudioRolloffModel model, float rolloff) {
  VoiceCmd cmd;
  cmd.kind = VoiceCmd::Kind::Update;
  cmd.sourceId = sourceId;
  cmd.fields = VoiceCmd::kFieldRolloff;
  cmd.rolloff = model;
  cmd.rolloffFactor = rolloff;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    commands.push_back(cmd);
  }
  return true;
}
void AudioEngine::SetListener(const float pos[3], const float forward[3], const float up[3]) {
  if (pos != nullptr) {
    listenerPos[0] = pos[0];
    listenerPos[1] = pos[1];
    listenerPos[2] = pos[2];
  }
  if (forward != nullptr) {
    listenerForward[0] = forward[0];
    listenerForward[1] = forward[1];
    listenerForward[2] = forward[2];
  }
  if (up != nullptr) {
    listenerUp[0] = up[0];
    listenerUp[1] = up[1];
    listenerUp[2] = up[2];
  }
}
void AudioEngine::Update() {
  std::lock_guard<std::mutex> lock(mixMutex);
  pendingMix.buses.clear();
  for (size_t i = 0; i < mixer.BusCount(); ++i) {
    const AudioBusDesc* b = mixer.GetBus(static_cast<uint32_t>(i));
    BusMix bm;
    bm.parent = b->parent;
    bm.volume = b->volume;
    bm.muted = b->muted;
    bm.soloed = b->soloed;
    bm.reverbEnabled = b->reverbEnabled;
    bm.reverbRoomSize = b->reverbRoomSize;
    bm.reverbDamping = b->reverbDamping;
    bm.reverbWidth = b->reverbWidth;
    bm.reverbWet = b->reverbWet;
    bm.ducking = b->ducking;
    pendingMix.buses.push_back(bm);
  }
  mixer.ComputeOrder(pendingMix.order);
}
float AudioEngine::ResolveLiveGain(uint32_t index) const {
  if (index >= liveMix.buses.size()) {
    return 0.0f;
  }
  const BusMix& b = liveMix.buses[index];
  float gain = b.volume;
  if (b.muted) {
    gain = 0.0f;
  }
  bool anySolo = false;
  for (size_t i = 1; i < liveMix.buses.size(); ++i) {
    if (liveMix.buses[i].soloed) {
      anySolo = true;
      break;
    }
  }
  if (anySolo && index != 0) {
    uint32_t cur = index;
    bool soloChain = false;
    int guard = 0;
    while (cur != 0xFFFFFFFFu && guard < 256) {
      if (liveMix.buses[cur].soloed) {
        soloChain = true;
        break;
      }
      const int parent = liveMix.buses[cur].parent;
      if (parent < 0) {
        break;
      }
      cur = static_cast<uint32_t>(parent);
      ++guard;
    }
    if (!soloChain) {
      gain = 0.0f;
    }
  }
  return gain;
}
float AudioEngine::ComputeAttenuation(float distance, float minDistance, float maxDistance, AudioRolloffModel model, float rolloff) {
  const float d = distance < 0.0f ? 0.0f : distance;
  const float dmin = minDistance > 0.01f ? minDistance : 0.01f;
  const float dmax = maxDistance > dmin ? maxDistance : dmin + 0.01f;
  if (d >= dmax) {
    return 0.0f;
  }
  switch (model) {
    case AudioRolloffModel::Linear: {
      const float t = (d - dmin) / (dmax - dmin);
      const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
      return 1.0f - clamped;
    }
    case AudioRolloffModel::Exponential: {
      const float factor = rolloff > 0.01f ? rolloff : 1.0f;
      const float dd = d < dmin ? dmin : d;
      return std::pow(dmin / dd, factor);
    }
    case AudioRolloffModel::Inverse:
    default: {
      const float factor = rolloff > 0.01f ? rolloff : 1.0f;
      const float dd = d < dmin ? dmin : d;
      return dmin / (dmin + factor * (dd - dmin));
    }
  }
}
void AudioEngine::EnsureBuffers(size_t busCount, uint32_t frames) {
  if (busBuffers.size() < busCount) {
    busBuffers.resize(busCount);
    busActivity.resize(busCount, 0.0f);
    duckFactors.resize(busCount, 1.0f);
    reverbs.resize(busCount);
    for (size_t i = 0; i < reverbs.size(); ++i) {
      if (!reverbs[i]) {
        reverbs[i].reset(new AudioReverb());
      }
    }
  }
  const size_t needed = static_cast<size_t>(frames) * channels;
  for (size_t i = 0; i < busBuffers.size(); ++i) {
    if (busBuffers[i].size() < needed) {
      busBuffers[i].assign(needed, 0.0f);
    }
  }
  if (scratch.size() < needed) {
    scratch.assign(needed, 0.0f);
  }
}
void AudioEngine::ApplyCommands() {
  std::vector<VoiceCmd> pending;
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    pending.swap(commands);
  }
  for (size_t i = 0; i < pending.size(); ++i) {
    const VoiceCmd& cmd = pending[i];
    if (cmd.kind == VoiceCmd::Kind::Add) {
      Voice v;
      v.sourceId = cmd.sourceId;
      if (cmd.clipId < clips.size()) {
        v.clip = clips[cmd.clipId];
      }
      v.busId = cmd.busId;
      v.gain = cmd.gain;
      v.playing = cmd.playing;
      v.spatial = cmd.spatial;
      v.loop = cmd.loop;
      v.rolloff = cmd.rolloff;
      v.rolloffFactor = cmd.rolloffFactor;
      v.position[0] = cmd.position[0];
      v.position[1] = cmd.position[1];
      v.position[2] = cmd.position[2];
      voices.push_back(v);
    } else if (cmd.kind == VoiceCmd::Kind::Remove) {
      for (size_t v = 0; v < voices.size(); ++v) {
        if (voices[v].sourceId == cmd.sourceId) {
          voices.erase(voices.begin() + static_cast<long>(v));
          break;
        }
      }
    } else {
      for (size_t v = 0; v < voices.size(); ++v) {
        Voice& voice = voices[v];
        if (voice.sourceId != cmd.sourceId) {
          continue;
        }
        if ((cmd.fields & VoiceCmd::kFieldGain) != 0) {
          voice.gain = cmd.gain;
        }
        if ((cmd.fields & VoiceCmd::kFieldPlaying) != 0) {
          voice.playing = cmd.playing;
        }
        if ((cmd.fields & VoiceCmd::kFieldBus) != 0) {
          voice.busId = cmd.busId;
        }
        if ((cmd.fields & VoiceCmd::kFieldSpatial) != 0) {
          voice.spatial = cmd.spatial;
        }
        if ((cmd.fields & VoiceCmd::kFieldLoop) != 0) {
          voice.loop = cmd.loop;
        }
        if ((cmd.fields & VoiceCmd::kFieldRolloff) != 0) {
          voice.rolloff = cmd.rolloff;
          voice.rolloffFactor = cmd.rolloffFactor;
        }
        if ((cmd.fields & VoiceCmd::kFieldPosition) != 0) {
          voice.position[0] = cmd.position[0];
          voice.position[1] = cmd.position[1];
          voice.position[2] = cmd.position[2];
        }
        break;
      }
    }
  }
}
void AudioEngine::Render(float* outInterleaved, uint32_t frames) {
  if (!initialized || outInterleaved == nullptr || frames == 0) {
    return;
  }
  std::lock_guard<std::mutex> renderLock(renderMutex);
  ApplyCommands();
  {
    std::lock_guard<std::mutex> lock(mixMutex);
    liveMix.buses.swap(pendingMix.buses);
    liveMix.order.swap(pendingMix.order);
  }
  const size_t busCount = liveMix.buses.size();
  if (busCount == 0) {
    std::memset(outInterleaved, 0, static_cast<size_t>(frames) * channels * sizeof(float));
    return;
  }
  EnsureBuffers(busCount, frames);
  const size_t sampleCount = static_cast<size_t>(frames) * channels;
  for (size_t b = 0; b < busCount; ++b) {
    std::fill(busBuffers[b].begin(), busBuffers[b].begin() + sampleCount, 0.0f);
    busActivity[b] = 0.0f;
    duckFactors[b] = 1.0f;
  }
  float rightVec[3];
  {
    const float* f = listenerForward;
    const float* u = listenerUp;
    rightVec[0] = u[1] * f[2] - u[2] * f[1];
    rightVec[1] = u[2] * f[0] - u[0] * f[2];
    rightVec[2] = u[0] * f[1] - u[1] * f[0];
    const float len = std::sqrt(rightVec[0] * rightVec[0] + rightVec[1] * rightVec[1] + rightVec[2] * rightVec[2]);
    if (len > 1e-6f) {
      rightVec[0] /= len;
      rightVec[1] /= len;
      rightVec[2] /= len;
    } else {
      rightVec[0] = 1.0f;
      rightVec[1] = 0.0f;
      rightVec[2] = 0.0f;
    }
  }
  for (size_t vi = 0; vi < voices.size(); ++vi) {
    Voice& voice = voices[vi];
    if (!voice.playing || !voice.clip || voice.clip->frameCount == 0) {
      continue;
    }
    if (voice.busId >= busCount) {
      continue;
    }
    const AudioClip& clip = *voice.clip;
    float gain = voice.gain;
    float panL = 0.70710678f;
    float panR = 0.70710678f;
    if (voice.spatial) {
      float dir[3];
      dir[0] = voice.position[0] - listenerPos[0];
      dir[1] = voice.position[1] - listenerPos[1];
      dir[2] = voice.position[2] - listenerPos[2];
      const float dist = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
      gain *= ComputeAttenuation(dist, voice.minDistance, voice.maxDistance, voice.rolloff, voice.rolloffFactor);
      if (dist > 1e-6f) {
        const float inv = 1.0f / dist;
        const float lateral = (dir[0] * rightVec[0] + dir[1] * rightVec[1] + dir[2] * rightVec[2]) * inv;
        const float clamped = lateral < -1.0f ? -1.0f : (lateral > 1.0f ? 1.0f : lateral);
        const float pan = 0.5f * (1.0f + clamped);
        panL = std::sqrt(1.0f - pan);
        panR = std::sqrt(pan);
      }
    }
    if (gain <= 0.0f) {
      busActivity[voice.busId] = 1.0f;
      continue;
    }
    const double step = clip.sampleRate > 0 ? static_cast<double>(sampleRate) / static_cast<double>(clip.sampleRate) : 1.0;
    double cursor = static_cast<double>(voice.cursor);
    std::vector<float>& target = busBuffers[voice.busId];
    for (uint32_t f = 0; f < frames; ++f) {
      if (cursor >= static_cast<double>(clip.frameCount)) {
        if (voice.loop) {
          cursor = 0.0;
          voice.cursor = 0;
        } else {
          voice.playing = false;
          break;
        }
      }
      const uint64_t index = static_cast<uint64_t>(cursor);
      const uint64_t indexNext = index + 1 < clip.frameCount ? index + 1 : index;
      const float frac = static_cast<float>(cursor - static_cast<double>(index));
      float l = 0.0f;
      float r = 0.0f;
      if (clip.channels >= 2) {
        const float a0 = clip.samples[static_cast<size_t>(index) * 2 + 0];
        const float a1 = clip.samples[static_cast<size_t>(indexNext) * 2 + 0];
        const float b0 = clip.samples[static_cast<size_t>(index) * 2 + 1];
        const float b1 = clip.samples[static_cast<size_t>(indexNext) * 2 + 1];
        l = a0 + (a1 - a0) * frac;
        r = b0 + (b1 - b0) * frac;
      } else if (clip.channels == 1) {
        const float a0 = clip.samples[index];
        const float a1 = clip.samples[indexNext];
        l = a0 + (a1 - a0) * frac;
        r = l;
      }
      target[static_cast<size_t>(f) * channels + 0] += l * gain * panL;
      if (channels > 1) {
        target[static_cast<size_t>(f) * channels + 1] += r * gain * panR;
      }
      cursor += step;
    }
    voice.cursor = static_cast<uint64_t>(cursor);
    busActivity[voice.busId] = 1.0f;
  }
  for (size_t b = 1; b < busCount; ++b) {
    const int parent = liveMix.buses[b].parent;
    if (parent >= 0 && static_cast<size_t>(parent) < busCount) {
      busActivity[static_cast<size_t>(parent)] = std::max(busActivity[static_cast<size_t>(parent)], busActivity[b]);
    }
  }
  for (size_t b = 0; b < busCount; ++b) {
    for (size_t d = 0; d < liveMix.buses[b].ducking.size(); ++d) {
      const AudioDuckLink& link = liveMix.buses[b].ducking[d];
      if (link.targetBus < 0 || static_cast<size_t>(link.targetBus) >= busCount) {
        continue;
      }
      if (busActivity[b] <= 0.0f) {
        continue;
      }
      const float factor = std::pow(10.0f, link.amountDb / 20.0f);
      const size_t target = static_cast<size_t>(link.targetBus);
      duckFactors[target] = std::min(duckFactors[target], factor);
    }
  }
  for (size_t oi = 0; oi < liveMix.order.size(); ++oi) {
    const uint32_t busIndex = liveMix.order[oi];
    if (busIndex >= busCount) {
      continue;
    }
    std::vector<float>& buf = busBuffers[busIndex];
    const float gain = ResolveLiveGain(busIndex) * duckFactors[busIndex];
    for (size_t i = 0; i < sampleCount; ++i) {
      buf[i] *= gain;
    }
    const BusMix& bm = liveMix.buses[busIndex];
    if (bm.reverbEnabled && reverbs[busIndex]) {
      AudioReverb& rev = *reverbs[busIndex];
      rev.Configure(sampleRate, bm.reverbRoomSize, bm.reverbDamping, bm.reverbWidth);
      rev.Process(buf.data(), scratch.data(), frames, channels, bm.reverbWet);
      std::copy(scratch.begin(), scratch.begin() + sampleCount, buf.begin());
    }
    const int parent = bm.parent;
    if (parent >= 0 && static_cast<size_t>(parent) < busCount) {
      std::vector<float>& dest = busBuffers[static_cast<size_t>(parent)];
      for (size_t i = 0; i < sampleCount; ++i) {
        dest[i] += buf[i];
      }
    }
  }
  std::vector<float>& master = busBuffers[0];
  float peak = 0.0f;
  uint32_t active = 0;
  for (size_t i = 0; i < voices.size(); ++i) {
    if (voices[i].playing) {
      ++active;
    }
  }
  activeVoices.store(active, std::memory_order_relaxed);
  for (size_t i = 0; i < sampleCount; ++i) {
    const float v = master[i];
    outInterleaved[i] = v;
    const float a = v < 0.0f ? -v : v;
    if (a > peak) {
      peak = a;
    }
  }
  masterPeak = peak;
}
uint32_t AudioEngine::ActiveVoiceCount() const {
  return activeVoices.load(std::memory_order_relaxed);
}
}