#include "pixelpal/menu_audio.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>

namespace pixelpal {
namespace {

std::string trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() &&
         (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' ||
          value[start] == '\n')) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start &&
         (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' ||
          value[end - 1] == '\n')) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string unquote(const std::string& value) {
  const std::string trimmed = trim(value);
  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
    return trimmed.substr(1, trimmed.size() - 2);
  }
  return trimmed;
}

std::map<std::string, std::string> parse_key_value_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::map<std::string, std::string> parsed;
  std::string line;

  while (std::getline(input, line)) {
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }
    line = trim(line);
    if (line.empty() || line.front() == '[') {
      continue;
    }
    const std::size_t separator = line.find('=');
    if (separator == std::string::npos) {
      continue;
    }
    parsed[trim(line.substr(0, separator))] = trim(line.substr(separator + 1));
  }

  return parsed;
}

int16_t clamp_sample(int value) {
  if (value > 32767) {
    return 32767;
  }
  if (value < -32768) {
    return -32768;
  }
  return static_cast<int16_t>(value);
}

}  // namespace

MenuAudio::MenuAudio() = default;

MenuAudio::~MenuAudio() {
  shutdown();
}

bool MenuAudio::initialize(const std::filesystem::path& theme_root) {
  SDL_AudioSpec desired{};
  const auto manifest_path = theme_root / "audio" / "manifest.toml";

  if (SDL_WasInit(SDL_INIT_AUDIO) == 0U) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      return false;
    }
    owns_audio_init_ = true;
  }

  desired.freq = 48000;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 1024;
  desired.callback = &MenuAudio::audio_callback;
  desired.userdata = this;
  device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &device_spec_, 0);
  if (device_ == 0U) {
    shutdown();
    return false;
  }

  available_ = load_theme_manifest(manifest_path);
  SDL_PauseAudioDevice(device_, 0);
  return available_;
}

void MenuAudio::play_move() {
  play_clip(move_);
}

void MenuAudio::play_confirm() {
  play_clip(confirm_);
}

void MenuAudio::play_back() {
  play_clip(back_);
}

bool MenuAudio::load_theme_manifest(const std::filesystem::path& manifest_path) {
  if (!std::filesystem::exists(manifest_path)) {
    return false;
  }

  const auto parsed = parse_key_value_file(manifest_path);
  const auto root = manifest_path.parent_path();

  if (parsed.count("menu_music") != 0) {
    load_clip_from_path(root / unquote(parsed.at("menu_music")), &music_);
  }
  if (parsed.count("menu_move") != 0) {
    load_clip_from_path(root / unquote(parsed.at("menu_move")), &move_);
  }
  if (parsed.count("menu_confirm") != 0) {
    load_clip_from_path(root / unquote(parsed.at("menu_confirm")), &confirm_);
  }
  if (parsed.count("menu_back") != 0) {
    load_clip_from_path(root / unquote(parsed.at("menu_back")), &back_);
  }

  return !music_.data.empty() || !move_.data.empty() || !confirm_.data.empty() ||
         !back_.data.empty();
}

bool MenuAudio::load_clip_from_path(const std::filesystem::path& path, AudioClip* clip) {
  SDL_AudioSpec source_spec{};
  Uint8* buffer = nullptr;
  Uint32 length = 0;
  SDL_AudioCVT converter{};
  int build_result = 0;

  if (clip == nullptr || !std::filesystem::exists(path)) {
    return false;
  }

  if (SDL_LoadWAV(path.string().c_str(), &source_spec, &buffer, &length) == nullptr) {
    return false;
  }

  build_result = SDL_BuildAudioCVT(&converter,
                                   source_spec.format,
                                   source_spec.channels,
                                   source_spec.freq,
                                   device_spec_.format,
                                   device_spec_.channels,
                                   device_spec_.freq);
  if (build_result < 0) {
    SDL_FreeWAV(buffer);
    return false;
  }

  converter.len = static_cast<int>(length);
  converter.buf = static_cast<Uint8*>(SDL_malloc(length * converter.len_mult));
  if (converter.buf == nullptr) {
    SDL_FreeWAV(buffer);
    return false;
  }

  SDL_memcpy(converter.buf, buffer, length);
  SDL_FreeWAV(buffer);

  if (SDL_ConvertAudio(&converter) != 0) {
    SDL_free(converter.buf);
    return false;
  }

  clip->data.assign(converter.buf, converter.buf + converter.len_cvt);
  SDL_free(converter.buf);
  return !clip->data.empty();
}

void MenuAudio::play_clip(const AudioClip& clip) {
  if (!available_ || device_ == 0U || clip.data.empty()) {
    return;
  }

  SDL_LockAudioDevice(device_);
  for (auto& channel : channels_) {
    if (channel.clip == nullptr) {
      channel.clip = &clip;
      channel.position = 0;
      SDL_UnlockAudioDevice(device_);
      return;
    }
  }

  channels_[0].clip = &clip;
  channels_[0].position = 0;
  SDL_UnlockAudioDevice(device_);
}

void MenuAudio::shutdown() {
  if (device_ != 0U) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  available_ = false;
  music_.data.clear();
  move_.data.clear();
  confirm_.data.clear();
  back_.data.clear();
  music_position_ = 0;
  for (auto& channel : channels_) {
    channel.clip = nullptr;
    channel.position = 0;
  }
  if (owns_audio_init_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_audio_init_ = false;
  }
}

void MenuAudio::audio_callback(void* userdata, Uint8* stream, int length) {
  MenuAudio* audio = static_cast<MenuAudio*>(userdata);
  if (audio == nullptr) {
    SDL_memset(stream, 0, length);
    return;
  }
  audio->mix_audio(stream, length);
}

void MenuAudio::mix_audio(Uint8* stream, int length) {
  int sample_index = 0;
  int16_t* output = reinterpret_cast<int16_t*>(stream);
  const int sample_count = length / static_cast<int>(sizeof(int16_t));

  SDL_memset(stream, 0, length);

  for (sample_index = 0; sample_index < sample_count; ++sample_index) {
    int mixed = 0;

    if (!music_.data.empty()) {
      const auto* music_samples = reinterpret_cast<const int16_t*>(music_.data.data());
      mixed += music_samples[music_position_ / sizeof(int16_t)];
      music_position_ += sizeof(int16_t);
      if (music_position_ >= music_.data.size()) {
        music_position_ = 0;
      }
    }

    for (auto& channel : channels_) {
      if (channel.clip == nullptr || channel.clip->data.empty()) {
        continue;
      }
      mixed += reinterpret_cast<const int16_t*>(channel.clip->data.data())
          [channel.position / sizeof(int16_t)];
      channel.position += sizeof(int16_t);
      if (channel.position >= channel.clip->data.size()) {
        channel.clip = nullptr;
        channel.position = 0;
      }
    }

    output[sample_index] = clamp_sample(mixed);
  }
}

}  // namespace pixelpal
