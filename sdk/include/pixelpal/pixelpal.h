#ifndef PIXELPAL_PIXELPAL_H
#define PIXELPAL_PIXELPAL_H

#include <SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PP_PATH_CAPACITY 512
#define PP_GAME_ID_CAPACITY 64

typedef struct pp_input_state {
  int up;
  int down;
  int left;
  int right;
  int a;
  int b;
  int start;
  int select;
} pp_input_state;

typedef struct pp_context {
  char game_id[PP_GAME_ID_CAPACITY];
  char save_dir[PP_PATH_CAPACITY];
  char config_dir[PP_PATH_CAPACITY];
  int framebuffer_width;
  int framebuffer_height;
  int exit_requested;
  uint32_t exit_combo_started_at;
  SDL_GameController* controller;
  pp_input_state input;
} pp_context;

typedef struct pp_audio_spec {
  int freq;
  SDL_AudioFormat format;
  uint8_t channels;
  uint16_t samples;
  SDL_AudioCallback callback;
  void* userdata;
} pp_audio_spec;

int pp_init(pp_context* context, const char* game_id);
void pp_shutdown(pp_context* context);
void pp_get_framebuffer_size(const pp_context* context, int* width, int* height);
const char* pp_get_save_dir(const pp_context* context);
const char* pp_get_config_dir(const pp_context* context);
void pp_request_exit(pp_context* context);
int pp_should_exit(const pp_context* context);
int pp_poll_input(pp_context* context, pp_input_state* output);
int pp_audio_open(const pp_audio_spec* spec, SDL_AudioDeviceID* device_out);

#ifdef __cplusplus
}
#endif

#endif
