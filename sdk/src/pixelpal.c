#include "pixelpal/pixelpal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define PP_PATH_SEPARATOR '\\'
#define PP_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define PP_PATH_SEPARATOR '/'
#define PP_MKDIR(path) mkdir(path, 0755)
#endif

static void pp_copy_string(char* destination, size_t capacity, const char* source) {
  if (capacity == 0) {
    return;
  }

  if (source == NULL) {
    destination[0] = '\0';
    return;
  }

  snprintf(destination, capacity, "%s", source);
}

static void pp_make_dir(const char* path) {
  size_t length = 0;
  char buffer[PP_PATH_CAPACITY];
  size_t index = 0;
  size_t start_index = 1;

  if (path == NULL || path[0] == '\0') {
    return;
  }

  length = strlen(path);
  if (length >= sizeof(buffer)) {
    return;
  }

  snprintf(buffer, sizeof(buffer), "%s", path);
  if (length >= 3 && buffer[1] == ':' &&
      (buffer[2] == '\\' || buffer[2] == '/')) {
    start_index = 3;
  }

  for (index = start_index; index < length; ++index) {
    if (buffer[index] == '/' || buffer[index] == '\\') {
      char original = buffer[index];
      buffer[index] = '\0';
      PP_MKDIR(buffer);
      buffer[index] = original;
    }
  }
  PP_MKDIR(buffer);
}

static void pp_join_path(char* output,
                         size_t capacity,
                         const char* root,
                         const char* game_id) {
  snprintf(output, capacity, "%s%c%s", root, PP_PATH_SEPARATOR, game_id);
}

static int pp_env_int(const char* name, int fallback) {
  const char* raw = getenv(name);
  if (raw == NULL || raw[0] == '\0') {
    return fallback;
  }
  return atoi(raw);
}

int pp_init(pp_context* context, const char* game_id) {
#if defined(_WIN32)
  char local_save_root[PP_PATH_CAPACITY];
  char local_config_root[PP_PATH_CAPACITY];
#endif
  const char* save_root = getenv("PIXELPAL_SAVE_ROOT");
  const char* config_root = getenv("PIXELPAL_GAME_CONFIG_ROOT");

  if (context == NULL || game_id == NULL || game_id[0] == '\0') {
    return -1;
  }

  memset(context, 0, sizeof(*context));
  pp_copy_string(context->game_id, sizeof(context->game_id), game_id);
  context->framebuffer_width = pp_env_int("PIXELPAL_FRAMEBUFFER_WIDTH", 512);
  context->framebuffer_height = pp_env_int("PIXELPAL_FRAMEBUFFER_HEIGHT", 512);

  if (save_root == NULL || save_root[0] == '\0') {
#if defined(_WIN32)
    const char* local_app_data = getenv("LOCALAPPDATA");
    if (local_app_data == NULL || local_app_data[0] == '\0') {
      local_app_data = ".";
    }
    snprintf(local_save_root, sizeof(local_save_root), "%s\\PixelPal\\saves", local_app_data);
    save_root = local_save_root;
#else
    save_root = "/var/lib/pixelpal/saves";
#endif
  }
  if (config_root == NULL || config_root[0] == '\0') {
#if defined(_WIN32)
    const char* local_app_data = getenv("LOCALAPPDATA");
    if (local_app_data == NULL || local_app_data[0] == '\0') {
      local_app_data = ".";
    }
    snprintf(local_config_root, sizeof(local_config_root), "%s\\PixelPal\\config", local_app_data);
    config_root = local_config_root;
#else
    config_root = "/var/lib/pixelpal/config";
#endif
  }

#if !defined(_WIN32)
  pp_make_dir("/var/lib/pixelpal");
#endif
  pp_make_dir(save_root);
  pp_make_dir(config_root);
  pp_join_path(context->save_dir, sizeof(context->save_dir), save_root, game_id);
  pp_join_path(context->config_dir, sizeof(context->config_dir), config_root, game_id);
  pp_make_dir(context->save_dir);
  pp_make_dir(context->config_dir);

  if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0U) {
    int joystick_count = SDL_NumJoysticks();
    int index = 0;
    while (index < joystick_count) {
      if (SDL_IsGameController(index) == SDL_TRUE) {
        context->controller = SDL_GameControllerOpen(index);
        break;
      }
      ++index;
    }
  }

  return 0;
}

void pp_shutdown(pp_context* context) {
  if (context != NULL && context->controller != NULL) {
    SDL_GameControllerClose(context->controller);
    context->controller = NULL;
  }
}

void pp_get_framebuffer_size(const pp_context* context, int* width, int* height) {
  if (context == NULL) {
    return;
  }
  if (width != NULL) {
    *width = context->framebuffer_width;
  }
  if (height != NULL) {
    *height = context->framebuffer_height;
  }
}

const char* pp_get_save_dir(const pp_context* context) {
  if (context == NULL) {
    return "";
  }
  return context->save_dir;
}

const char* pp_get_config_dir(const pp_context* context) {
  if (context == NULL) {
    return "";
  }
  return context->config_dir;
}

void pp_request_exit(pp_context* context) {
  if (context != NULL) {
    context->exit_requested = 1;
  }
}

int pp_should_exit(const pp_context* context) {
  if (context == NULL) {
    return 1;
  }
  return context->exit_requested;
}

static void pp_update_button(int pressed, int* target) {
  if (target != NULL) {
    *target = pressed;
  }
}

static void pp_process_keyboard(pp_context* context, const SDL_KeyboardEvent* event) {
  const int pressed = event->type == SDL_KEYDOWN;
  switch (event->keysym.sym) {
    case SDLK_UP:
      pp_update_button(pressed, &context->input.up);
      break;
    case SDLK_DOWN:
      pp_update_button(pressed, &context->input.down);
      break;
    case SDLK_LEFT:
      pp_update_button(pressed, &context->input.left);
      break;
    case SDLK_RIGHT:
      pp_update_button(pressed, &context->input.right);
      break;
    case SDLK_z:
    case SDLK_a:
    case SDLK_c:
      pp_update_button(pressed, &context->input.a);
      break;
    case SDLK_b:
    case SDLK_x:
      pp_update_button(pressed, &context->input.b);
      break;
    case SDLK_RETURN:
      pp_update_button(pressed, &context->input.start);
      break;
    case SDLK_SPACE:
    case SDLK_RSHIFT:
    case SDLK_BACKSPACE:
      pp_update_button(pressed, &context->input.select);
      break;
    case SDLK_ESCAPE:
      if (pressed) {
        context->exit_requested = 1;
      }
      break;
    default:
      break;
  }
}

static void pp_process_controller(pp_context* context, const SDL_ControllerButtonEvent* event) {
  const int pressed = event->type == SDL_CONTROLLERBUTTONDOWN;
  switch (event->button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      pp_update_button(pressed, &context->input.up);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      pp_update_button(pressed, &context->input.down);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      pp_update_button(pressed, &context->input.left);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      pp_update_button(pressed, &context->input.right);
      break;
    case SDL_CONTROLLER_BUTTON_A:
      pp_update_button(pressed, &context->input.a);
      break;
    case SDL_CONTROLLER_BUTTON_B:
      pp_update_button(pressed, &context->input.b);
      break;
    case SDL_CONTROLLER_BUTTON_START:
      pp_update_button(pressed, &context->input.start);
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      pp_update_button(pressed, &context->input.select);
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      break;
    default:
      break;
  }
}

static void pp_check_exit_combo(pp_context* context) {
  if (context->input.start && context->input.select) {
    if (context->exit_combo_started_at == 0) {
      context->exit_combo_started_at = SDL_GetTicks();
    } else if (SDL_GetTicks() - context->exit_combo_started_at >= 1200U) {
      context->exit_requested = 1;
    }
  } else {
    context->exit_combo_started_at = 0;
  }
}

int pp_poll_input(pp_context* context, pp_input_state* output) {
  SDL_Event event;

  if (context == NULL) {
    return -1;
  }

  while (SDL_PollEvent(&event) == 1) {
    switch (event.type) {
      case SDL_QUIT:
        context->exit_requested = 1;
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        pp_process_keyboard(context, &event.key);
        break;
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
        pp_process_controller(context, &event.cbutton);
        break;
      default:
        break;
    }
  }

  pp_check_exit_combo(context);

  if (output != NULL) {
    *output = context->input;
  }

  return 0;
}

int pp_audio_open(const pp_audio_spec* spec, SDL_AudioDeviceID* device_out) {
  SDL_AudioSpec desired;
  SDL_AudioSpec obtained;
  SDL_AudioDeviceID device;

  if (spec == NULL || device_out == NULL) {
    return -1;
  }

  memset(&desired, 0, sizeof(desired));
  desired.freq = spec->freq;
  desired.format = spec->format;
  desired.channels = spec->channels;
  desired.samples = spec->samples;
  desired.callback = spec->callback;
  desired.userdata = spec->userdata;

  device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
  if (device == 0U) {
    return -1;
  }

  *device_out = device;
  return 0;
}
