#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define SLINKBIT_PATH_SEPARATOR '\\'
#else
#define SLINKBIT_PATH_SEPARATOR '/'
#endif

#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512
#define GRID_WIDTH 18
#define GRID_HEIGHT 18
#define CELL_SIZE 20
#define BOARD_OFFSET_X 76
#define BOARD_OFFSET_Y 76
#define MAX_SEGMENTS (GRID_WIDTH * GRID_HEIGHT)

typedef enum direction {
  DIR_UP = 0,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
} direction;

typedef struct point {
  int x;
  int y;
} point;

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

static const SDL_Color k_bg = {218, 223, 184, 255};
static const SDL_Color k_panel = {194, 201, 159, 255};
static const SDL_Color k_shadow = {146, 156, 114, 255};
static const SDL_Color k_dark = {23, 56, 28, 255};
static const SDL_Color k_grid = {170, 182, 132, 255};
static const SDL_Color k_wire = {70, 121, 85, 255};
static const SDL_Color k_wire_head = {96, 151, 112, 255};
static const SDL_Color k_packet = {245, 192, 88, 255};
static const SDL_Color k_packet_core = {255, 229, 168, 255};
static const SDL_Color k_muted = {74, 104, 63, 255};

static void audio_callback(void* userdata, Uint8* stream, int length) {
  tone_state* state = (tone_state*)userdata;
  int16_t* samples = (int16_t*)stream;
  int count = length / (int)sizeof(int16_t);
  int index = 0;
  for (index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (state->frames_remaining > 0) {
      sample = (state->phase < 3.14159f) ? 1600 : -1600;
      state->phase += (6.28318f * state->frequency) / 48000.0f;
      if (state->phase >= 6.28318f) {
        state->phase -= 6.28318f;
      }
      --state->frames_remaining;
    }
    samples[index] = sample;
  }
}

static void trigger_tone(tone_state* tone, float frequency, int milliseconds) {
  tone->frequency = frequency;
  tone->frames_remaining = (48000 * milliseconds) / 1000;
}

static void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

static int maximum(int left, int right) {
  return left > right ? left : right;
}

static int text_width(const char* text, int scale) {
  if (text == NULL || text[0] == '\0') {
    return 0;
  }
  return (int)strlen(text) * (6 * scale) - scale;
}

static uint8_t glyph_row(char ch, int row) {
  static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t a[7] = {14, 17, 17, 31, 17, 17, 17};
  static const uint8_t b[7] = {30, 17, 17, 30, 17, 17, 30};
  static const uint8_t c[7] = {14, 17, 16, 16, 16, 17, 14};
  static const uint8_t d[7] = {28, 18, 17, 17, 17, 18, 28};
  static const uint8_t e[7] = {31, 16, 16, 30, 16, 16, 31};
  static const uint8_t f[7] = {31, 16, 16, 30, 16, 16, 16};
  static const uint8_t g[7] = {14, 17, 16, 23, 17, 17, 14};
  static const uint8_t h[7] = {17, 17, 17, 31, 17, 17, 17};
  static const uint8_t i[7] = {31, 4, 4, 4, 4, 4, 31};
  static const uint8_t k[7] = {17, 18, 20, 24, 20, 18, 17};
  static const uint8_t l[7] = {16, 16, 16, 16, 16, 16, 31};
  static const uint8_t m[7] = {17, 27, 21, 21, 17, 17, 17};
  static const uint8_t n[7] = {17, 17, 25, 21, 19, 17, 17};
  static const uint8_t o[7] = {14, 17, 17, 17, 17, 17, 14};
  static const uint8_t p[7] = {30, 17, 17, 30, 16, 16, 16};
  static const uint8_t q[7] = {14, 17, 17, 17, 21, 18, 13};
  static const uint8_t r[7] = {30, 17, 17, 30, 20, 18, 17};
  static const uint8_t s[7] = {15, 16, 16, 14, 1, 1, 30};
  static const uint8_t t[7] = {31, 4, 4, 4, 4, 4, 4};
  static const uint8_t u[7] = {17, 17, 17, 17, 17, 17, 14};
  static const uint8_t v[7] = {17, 17, 17, 17, 17, 10, 4};
  static const uint8_t x[7] = {17, 17, 10, 4, 10, 17, 17};
  static const uint8_t y[7] = {17, 17, 10, 4, 4, 4, 4};
  static const uint8_t n0[7] = {14, 17, 19, 21, 25, 17, 14};
  static const uint8_t n1[7] = {4, 12, 20, 4, 4, 4, 31};
  static const uint8_t n2[7] = {14, 17, 1, 2, 4, 8, 31};
  static const uint8_t n3[7] = {30, 1, 1, 6, 1, 1, 30};
  static const uint8_t n4[7] = {2, 6, 10, 18, 31, 2, 2};
  static const uint8_t n5[7] = {31, 16, 16, 30, 1, 1, 30};
  static const uint8_t n6[7] = {14, 16, 16, 30, 17, 17, 14};
  static const uint8_t n7[7] = {31, 1, 2, 4, 8, 8, 8};
  static const uint8_t n8[7] = {14, 17, 17, 14, 17, 17, 14};
  static const uint8_t n9[7] = {14, 17, 17, 15, 1, 1, 14};
  static const uint8_t colon[7] = {0, 12, 12, 0, 12, 12, 0};
  static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};

  const uint8_t* glyph = blank;
  switch (ch) {
    case 'A': glyph = a; break; case 'B': glyph = b; break; case 'C': glyph = c; break;
    case 'D': glyph = d; break; case 'E': glyph = e; break; case 'F': glyph = f; break;
    case 'G': glyph = g; break; case 'H': glyph = h; break; case 'I': glyph = i; break;
    case 'K': glyph = k; break; case 'L': glyph = l; break; case 'M': glyph = m; break;
    case 'N': glyph = n; break; case 'O': glyph = o; break; case 'P': glyph = p; break;
    case 'Q': glyph = q; break; case 'R': glyph = r; break; case 'S': glyph = s; break;
    case 'T': glyph = t; break; case 'U': glyph = u; break; case 'V': glyph = v; break;
    case 'X': glyph = x; break; case 'Y': glyph = y; break; case '0': glyph = n0; break; case '1': glyph = n1; break;
    case '2': glyph = n2; break; case '3': glyph = n3; break; case '4': glyph = n4; break;
    case '5': glyph = n5; break; case '6': glyph = n6; break; case '7': glyph = n7; break;
    case '8': glyph = n8; break; case '9': glyph = n9; break; case ':': glyph = colon; break;
    case '-': glyph = dash; break; default: break;
  }
  return glyph[row];
}

static void draw_text(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color, int centered) {
  int draw_x = x;
  size_t index = 0;
  if (text == NULL) {
    return;
  }
  if (centered) {
    draw_x -= text_width(text, scale) / 2;
  }
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (index = 0; text[index] != '\0'; ++index) {
    char ch = text[index];
    int row = 0;
    if (ch >= 'a' && ch <= 'z') {
      ch = (char)(ch - ('a' - 'A'));
    }
    for (row = 0; row < 7; ++row) {
      uint8_t bits = glyph_row(ch, row);
      int column = 0;
      for (column = 0; column < 5; ++column) {
        if ((bits & (1 << (4 - column))) == 0) {
          continue;
        }
        SDL_Rect pixel = {draw_x + column * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    draw_x += 6 * scale;
  }
}

static void draw_text_right(SDL_Renderer* renderer,
                            const char* text,
                            int right_x,
                            int y,
                            int scale,
                            SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, 0);
}

static int direction_dx(direction dir) {
  if (dir == DIR_LEFT) {
    return -1;
  }
  if (dir == DIR_RIGHT) {
    return 1;
  }
  return 0;
}

static int direction_dy(direction dir) {
  if (dir == DIR_UP) {
    return -1;
  }
  if (dir == DIR_DOWN) {
    return 1;
  }
  return 0;
}

static int is_opposite(direction left, direction right) {
  return (left == DIR_UP && right == DIR_DOWN) || (left == DIR_DOWN && right == DIR_UP) ||
         (left == DIR_LEFT && right == DIR_RIGHT) || (left == DIR_RIGHT && right == DIR_LEFT);
}

static void save_high_score(const pp_context* context, int high_score) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), SLINKBIT_PATH_SEPARATOR);
  file = fopen(path, "w");
  if (file == NULL) {
    return;
  }
  fprintf(file, "%d\n", high_score);
  fclose(file);
}

static int load_high_score(const pp_context* context) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  int value = 0;
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), SLINKBIT_PATH_SEPARATOR);
  file = fopen(path, "r");
  if (file == NULL) {
    return 0;
  }
  if (fscanf(file, "%d", &value) != 1) {
    value = 0;
  }
  fclose(file);
  return value;
}

static int occupies(const point* segments, int length, int x, int y) {
  int index = 0;
  for (index = 0; index < length; ++index) {
    if (segments[index].x == x && segments[index].y == y) {
      return 1;
    }
  }
  return 0;
}

static void spawn_packet(point* packet, const point* segments, int length) {
  do {
    packet->x = rand() % GRID_WIDTH;
    packet->y = rand() % GRID_HEIGHT;
  } while (occupies(segments, length, packet->x, packet->y));
}

static void reset_game(point* segments,
                       int* length,
                       direction* dir,
                       direction* next_dir,
                       point* packet,
                       int* score,
                       int* paused,
                       int* game_over) {
  *length = 4;
  segments[0] = (point){9, 9};
  segments[1] = (point){8, 9};
  segments[2] = (point){7, 9};
  segments[3] = (point){6, 9};
  *dir = DIR_RIGHT;
  *next_dir = DIR_RIGHT;
  spawn_packet(packet, segments, *length);
  *score = 0;
  *paused = 0;
  *game_over = 0;
}

static void draw_segment(SDL_Renderer* renderer, const point* segment, int head, direction dir) {
  SDL_Rect body = {BOARD_OFFSET_X + segment->x * CELL_SIZE + 2, BOARD_OFFSET_Y + segment->y * CELL_SIZE + 2,
                   CELL_SIZE - 4, CELL_SIZE - 4};
  SDL_Rect core = {body.x + 4, body.y + 4, body.w - 8, body.h - 8};
  SDL_Rect nose = {body.x + body.w - 4, body.y + 3, 3, body.h - 6};
  fill_rect(renderer, body, head ? k_wire_head : k_wire);
  fill_rect(renderer, core, k_panel);
  if (head) {
    if (dir == DIR_LEFT) {
      nose = (SDL_Rect){body.x + 1, body.y + 3, 3, body.h - 6};
    } else if (dir == DIR_UP) {
      nose = (SDL_Rect){body.x + 3, body.y + 1, body.w - 6, 3};
    } else if (dir == DIR_DOWN) {
      nose = (SDL_Rect){body.x + 3, body.y + body.h - 4, body.w - 6, 3};
    }
    fill_rect(renderer, nose, k_packet_core);
    if (dir == DIR_LEFT || dir == DIR_RIGHT) {
      fill_rect(renderer, (SDL_Rect){body.x + body.w / 2 - 1, body.y + 5, 2, 2}, k_dark);
      fill_rect(renderer, (SDL_Rect){body.x + body.w / 2 - 1, body.y + body.h - 7, 2, 2}, k_dark);
    } else {
      fill_rect(renderer, (SDL_Rect){body.x + 5, body.y + body.h / 2 - 1, 2, 2}, k_dark);
      fill_rect(renderer, (SDL_Rect){body.x + body.w - 7, body.y + body.h / 2 - 1, 2, 2}, k_dark);
    }
  }
}

static void draw_packet(SDL_Renderer* renderer, const point* packet, Uint32 ticks) {
  SDL_Rect shell = {BOARD_OFFSET_X + packet->x * CELL_SIZE + 4, BOARD_OFFSET_Y + packet->y * CELL_SIZE + 4,
                    CELL_SIZE - 8, CELL_SIZE - 8};
  SDL_Rect core = {shell.x + 4, shell.y + 4, shell.w - 8, shell.h - 8};
  fill_rect(renderer, shell, k_packet);
  fill_rect(renderer, core, ((ticks / 200U) % 2U == 0U) ? k_packet_core : k_panel);
}

static void render_scene(SDL_Renderer* renderer,
                         const point* segments,
                         int length,
                         const point* packet,
                         int score,
                         int high_score,
                         int speed_level,
                         direction dir,
                         int paused,
                         int game_over,
                         Uint32 ticks) {
  SDL_Rect shadow = {BOARD_OFFSET_X - 12, BOARD_OFFSET_Y - 12, GRID_WIDTH * CELL_SIZE + 24, GRID_HEIGHT * CELL_SIZE + 24};
  SDL_Rect board = {BOARD_OFFSET_X - 16, BOARD_OFFSET_Y - 16, GRID_WIDTH * CELL_SIZE + 32, GRID_HEIGHT * CELL_SIZE + 32};
  int x = 0;
  int y = 0;
  int index = 0;
  char buffer[64];

  fill_rect(renderer, (SDL_Rect){0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT}, k_bg);
  fill_rect(renderer, shadow, k_shadow);
  fill_rect(renderer, board, k_panel);
  fill_rect(renderer, (SDL_Rect){BOARD_OFFSET_X, BOARD_OFFSET_Y, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE}, k_dark);

  draw_text(renderer, "SLINKBIT", VIRTUAL_WIDTH / 2, 20, 4, k_dark, 1);
  snprintf(buffer, sizeof(buffer), "SCORE %d", score);
  draw_text(renderer, buffer, 18, 30, 2, k_dark, 0);
  snprintf(buffer, sizeof(buffer), "BEST %d", high_score);
  draw_text_right(renderer, buffer, VIRTUAL_WIDTH - 18, 30, 2, k_dark);
  snprintf(buffer, sizeof(buffer), "SPEED %d", speed_level);
  draw_text(renderer, buffer, VIRTUAL_WIDTH / 2, 470, 2, k_muted, 1);

  for (y = 0; y < GRID_HEIGHT; ++y) {
    for (x = 0; x < GRID_WIDTH; ++x) {
      stroke_rect(renderer,
                  (SDL_Rect){BOARD_OFFSET_X + x * CELL_SIZE, BOARD_OFFSET_Y + y * CELL_SIZE, CELL_SIZE, CELL_SIZE},
                  k_grid);
    }
  }

  draw_packet(renderer, packet, ticks);
  for (index = length - 1; index >= 0; --index) {
    draw_segment(renderer, &segments[index], index == 0, dir);
  }

  draw_text(renderer, "HOLD START SELECT TO EXIT", VIRTUAL_WIDTH / 2, 492, 1, k_muted, 1);
  if (paused) {
    fill_rect(renderer, (SDL_Rect){102, 208, 308, 96}, (SDL_Color){24, 44, 26, 228});
    stroke_rect(renderer, (SDL_Rect){102, 208, 308, 96}, k_panel);
    draw_text(renderer, "PAUSED", VIRTUAL_WIDTH / 2, 224, 4, k_packet_core, 1);
    draw_text(renderer, "PRESS START TO RESUME", VIRTUAL_WIDTH / 2, 266, 2, k_packet_core, 1);
  }
  if (game_over) {
    fill_rect(renderer, (SDL_Rect){88, 200, 336, 112}, (SDL_Color){24, 44, 26, 228});
    stroke_rect(renderer, (SDL_Rect){88, 200, 336, 112}, k_panel);
    draw_text(renderer, "SIGNAL LOST", VIRTUAL_WIDTH / 2, 218, 4, k_packet_core, 1);
    draw_text(renderer, "PRESS A B OR START", VIRTUAL_WIDTH / 2, 266, 2, k_packet_core, 1);
  }
  SDL_RenderPresent(renderer);
}

int main(int argc, char** argv) {
  pp_context context;
  pp_input_state input;
  pp_input_state previous_input;
  tone_state tone;
  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  SDL_AudioDeviceID audio_device = 0;
  pp_audio_spec audio_spec;
  point segments[MAX_SEGMENTS];
  point packet;
  direction dir = DIR_RIGHT;
  direction next_dir = DIR_RIGHT;
  int length = 0;
  int score = 0;
  int high_score = 0;
  int paused = 0;
  int game_over = 0;
  int width = 0;
  int height = 0;
  Uint32 last_step = 0U;

  (void)argc;
  (void)argv;

  memset(&previous_input, 0, sizeof(previous_input));
  memset(&tone, 0, sizeof(tone));
  srand((unsigned int)time(NULL));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (pp_init(&context, "slinkbit") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(512, width);
  height = maximum(512, height);
  high_score = load_high_score(&context);
  reset_game(segments, &length, &dir, &next_dir, &packet, &score, &paused, &game_over);

  window = SDL_CreateWindow("Slinkbit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == NULL) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderSetLogicalSize(renderer, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);

  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &tone;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  last_step = SDL_GetTicks();
  while (!pp_should_exit(&context)) {
    Uint32 now = SDL_GetTicks();
    int speed_level = 1 + score / 5;
    int move_interval = maximum(70, 170 - (speed_level - 1) * 8);

    pp_poll_input(&context, &input);
    if (input.up && dir != DIR_DOWN) {
      next_dir = DIR_UP;
    } else if (input.down && dir != DIR_UP) {
      next_dir = DIR_DOWN;
    } else if (input.left && dir != DIR_RIGHT) {
      next_dir = DIR_LEFT;
    } else if (input.right && dir != DIR_LEFT) {
      next_dir = DIR_RIGHT;
    }

    if (input.start && !previous_input.start) {
      if (game_over) {
        reset_game(segments, &length, &dir, &next_dir, &packet, &score, &paused, &game_over);
        trigger_tone(&tone, 660.0f, 90);
      } else {
        paused = !paused;
        trigger_tone(&tone, paused ? 320.0f : 620.0f, 45);
      }
    }
    if (game_over && ((input.a && !previous_input.a) || (input.b && !previous_input.b))) {
      reset_game(segments, &length, &dir, &next_dir, &packet, &score, &paused, &game_over);
      trigger_tone(&tone, 660.0f, 90);
    }

    if (!paused && !game_over && now - last_step >= (Uint32)move_interval) {
      point new_head = segments[0];
      point tail = segments[length - 1];
      int index = 0;
      if (!is_opposite(dir, next_dir)) {
        dir = next_dir;
      }

      new_head.x += direction_dx(dir);
      new_head.y += direction_dy(dir);
      if (new_head.x < 0 || new_head.x >= GRID_WIDTH || new_head.y < 0 || new_head.y >= GRID_HEIGHT ||
          occupies(segments, length, new_head.x, new_head.y)) {
        game_over = 1;
        trigger_tone(&tone, 150.0f, 260);
      } else {
        for (index = length - 1; index > 0; --index) {
          segments[index] = segments[index - 1];
        }
        segments[0] = new_head;
        if (new_head.x == packet.x && new_head.y == packet.y) {
          if (length < MAX_SEGMENTS) {
            segments[length] = tail;
            ++length;
          }
          score += 1;
          if (score > high_score) {
            high_score = score;
            save_high_score(&context, high_score);
          }
          spawn_packet(&packet, segments, length);
          trigger_tone(&tone, 880.0f, 80);
        } else {
          trigger_tone(&tone, 420.0f, 18);
        }
      }
      last_step = now;
    }

    render_scene(renderer, segments, length, &packet, score, high_score, speed_level, dir, paused, game_over, now);
    previous_input = input;
    SDL_Delay(16);
  }

  save_high_score(&context, high_score);
  if (audio_device != 0U) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
