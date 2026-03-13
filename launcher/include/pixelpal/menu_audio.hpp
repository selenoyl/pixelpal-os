#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace pixelpal {

class MenuAudio {
 public:
  MenuAudio();
  ~MenuAudio();

  bool initialize(const std::filesystem::path& theme_root);
  void play_move();
  void play_confirm();
  void play_back();

 private:
  struct AudioClip {
    std::vector<Uint8> data;
  };

  struct AudioChannel {
    const AudioClip* clip = nullptr;
    std::size_t position = 0;
  };

  bool load_theme_manifest(const std::filesystem::path& manifest_path);
  bool load_clip_from_path(const std::filesystem::path& path, AudioClip* clip);
  void play_clip(const AudioClip& clip);
  void shutdown();

  static void audio_callback(void* userdata, Uint8* stream, int length);
  void mix_audio(Uint8* stream, int length);

  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec device_spec_{};
  bool owns_audio_init_ = false;
  bool available_ = false;

  AudioClip music_;
  AudioClip move_;
  AudioClip confirm_;
  AudioClip back_;
  std::size_t music_position_ = 0;
  std::array<AudioChannel, 4> channels_{};
};

}  // namespace pixelpal

