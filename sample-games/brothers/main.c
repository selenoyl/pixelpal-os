#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define BROTHERS_PATH_SEPARATOR '\\'
#else
#define BROTHERS_PATH_SEPARATOR '/'
#endif

#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512
#define STEP_MS 16

#define MAX_PLATFORMS 7
#define MAX_PLATFORM_SEGMENTS 16
#define MAX_ENEMIES 12
#define MAX_RELICS 12
#define MAX_WISPS 3
#define MAX_OFFERINGS 8
#define MAX_SPAWN_QUEUE 16

#define PLATFORM_HEIGHT 14
#define SEGMENT_WIDTH 32

#define PLAYER_W 20
#define PLAYER_H 24
#define ENEMY_W 20
#define ENEMY_H 20

#define FLOOR_Y 456
#define BELL_X 240
#define BELL_Y 400
#define BELL_W 32
#define BELL_H 28

typedef enum game_mode {
  MODE_TITLE = 0,
  MODE_PLAY,
  MODE_GAME_OVER
} game_mode;

typedef enum enemy_kind {
  ENEMY_IMP = 0,
  ENEMY_TALON,
  ENEMY_GARGLET
} enemy_kind;

typedef enum enemy_state {
  ENEMY_NORMAL = 0,
  ENEMY_STUNNED
} enemy_state;

typedef enum relic_kind {
  RELIC_CANDLE = 0,
  RELIC_CHALICE,
  RELIC_PSALTER
} relic_kind;

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

typedef struct palette_entry {
  char key;
  SDL_Color color;
} palette_entry;

typedef struct platform {
  SDL_Rect rect;
  int can_bump;
  int segment_count;
  Uint32 bump_started[MAX_PLATFORM_SEGMENTS];
} platform;

typedef struct player_state {
  float x;
  float y;
  float vx;
  float vy;
  int facing;
  int on_ground;
  int platform_index;
  int active;
  int monk_index;
  Uint32 invulnerable_until;
  Uint32 respawn_at;
} player_state;

typedef struct enemy {
  int active;
  enemy_kind kind;
  enemy_state state;
  float x;
  float y;
  float vx;
  float vy;
  int facing;
  int on_ground;
  int platform_index;
  int anger;
  int speed_tier;
  Uint32 stun_until;
  Uint32 flash_until;
  Uint32 next_hop_at;
} enemy;

typedef struct relic {
  int active;
  relic_kind kind;
  float x;
  float y;
  float vx;
  float vy;
  Uint32 expire_at;
} relic;

typedef struct wisp {
  int active;
  float x;
  float base_y;
  float vx;
  Uint32 born_at;
} wisp;

typedef struct offering {
  int active;
  float x;
  float y;
  relic_kind kind;
} offering;

typedef struct wave_recipe {
  int imps;
  int talons;
  int garglets;
} wave_recipe;

typedef struct frame_input {
  int left;
  int right;
  int jump_held;
  int jump_pressed;
  int start_pressed;
} frame_input;

typedef struct game_state {
  game_mode mode;
  Uint32 clock;
  platform platforms[MAX_PLATFORMS];
  player_state player;
  enemy enemies[MAX_ENEMIES];
  relic relics[MAX_RELICS];
  wisp wisps[MAX_WISPS];
  offering offerings[MAX_OFFERINGS];
  int score;
  int high_score;
  int lives;
  int phase;
  int combat_round;
  int next_extra_life;
  int next_monk_index;
  int bonus_round;
  int offerings_remaining;
  int bell_charges;
  int perfect_bonus;
  int paused;
  Uint32 bell_flash_until;
  Uint32 bell_cooldown_until;
  Uint32 banner_until;
  Uint32 clear_until;
  Uint32 round_started_at;
  Uint32 next_wisp_at;
  Uint32 next_spawn_at;
  enemy_kind spawn_queue[MAX_SPAWN_QUEUE];
  int spawn_queue_count;
  int spawn_queue_index;
  tone_state tone;
} game_state;

static const SDL_Color k_bg_top = {12, 17, 34, 255};
static const SDL_Color k_bg_bottom = {22, 14, 28, 255};
static const SDL_Color k_wall = {33, 40, 60, 255};
static const SDL_Color k_wall_shadow = {16, 20, 36, 255};
static const SDL_Color k_window = {86, 122, 201, 255};
static const SDL_Color k_window_glow = {176, 215, 255, 255};
static const SDL_Color k_window_red = {176, 73, 71, 255};
static const SDL_Color k_window_gold = {223, 182, 89, 255};
static const SDL_Color k_stone_top = {154, 165, 184, 255};
static const SDL_Color k_stone_mid = {104, 114, 134, 255};
static const SDL_Color k_stone_shadow = {62, 68, 87, 255};
static const SDL_Color k_floor_shadow = {34, 21, 26, 255};
static const SDL_Color k_gold = {228, 190, 96, 255};
static const SDL_Color k_gold_dark = {142, 101, 43, 255};
static const SDL_Color k_glow = {255, 225, 143, 255};
static const SDL_Color k_text = {243, 237, 221, 255};
static const SDL_Color k_muted = {167, 163, 183, 255};
static const SDL_Color k_overlay = {7, 8, 18, 220};
static const SDL_Color k_shadow = {9, 6, 12, 170};
static const SDL_Color k_wisp_core = {242, 247, 255, 255};
static const SDL_Color k_wisp_outer = {93, 233, 240, 255};
static const SDL_Color k_dull = {93, 98, 118, 255};
static const SDL_Color k_life = {237, 208, 186, 255};

static const SDL_Rect k_platform_layout[MAX_PLATFORMS] = {
    {176, 112, 160, PLATFORM_HEIGHT},
    {40, 192, 160, PLATFORM_HEIGHT},
    {312, 192, 160, PLATFORM_HEIGHT},
    {176, 272, 160, PLATFORM_HEIGHT},
    {40, 352, 160, PLATFORM_HEIGHT},
    {312, 352, 160, PLATFORM_HEIGHT},
    {0, FLOOR_Y, VIRTUAL_WIDTH, PLATFORM_HEIGHT},
};

static const wave_recipe k_wave_recipes[] = {
    {4, 0, 0},
    {3, 1, 0},
    {2, 1, 1},
    {3, 2, 0},
    {2, 2, 1},
    {1, 3, 1},
    {2, 2, 2},
    {1, 3, 2},
};

static const SDL_Point k_offering_points[MAX_OFFERINGS] = {
    {84, 164},  {256, 100}, {428, 164}, {116, 244},
    {396, 244}, {84, 388},  {428, 388}, {256, 408},
};

static const char* const k_monk_idle[] = {
    "..tttt....",
    ".tkkkkt...",
    ".kssssk...",
    ".ktsstk...",
    ".kRRRRk...",
    "kRccccRk..",
    "kRrrrrRk..",
    "kRrrrrRk..",
    ".krrrrk...",
    ".kr..rk...",
    "kr....rk..",
    "bb....bb..",
};

static const char* const k_monk_stride[] = {
    "..tttt....",
    ".tkkkkt...",
    ".kssssk...",
    ".ktsstk...",
    ".kRRRRk...",
    "kRccccRk..",
    "kRrrrrRk..",
    "kRrrrrRk..",
    ".krrrrk...",
    "kr.rr.k...",
    "b..rr..b..",
    ".b....b...",
};

static const char* const k_monk_jump[] = {
    "..tttt....",
    ".tkkkkt...",
    ".kssssk...",
    ".ktsstk...",
    "kRccccRk..",
    "kRrrrrRk..",
    "kRrrrrRk..",
    ".krrrrk...",
    "brkrrkrb..",
    "..kr.rk...",
    ".b....b...",
    "b......b..",
};

static const char* const k_imp_a[] = {
    "..hh.hh...",
    ".hRkkRh...",
    ".kRRRRk...",
    "kRryyRrk..",
    "kRRRRRRk..",
    ".kRkkRk...",
    "krR..Rrk..",
    "r.r..r.r..",
    "rr....rr..",
    ".r....r...",
};

static const char* const k_imp_b[] = {
    "..hh.hh...",
    ".hRkkRh...",
    ".kRRRRk...",
    "kRryyRrk..",
    "kRRRRRRk..",
    ".kRkkRk...",
    "krR..Rrk..",
    ".rr..rr...",
    "r.r..r.r..",
    ".r....r...",
};

static const char* const k_talon_a[] = {
    "..cc..cc..",
    ".cPkkPPc..",
    "ckPPPPPPk.",
    "kPPyyPPPk.",
    "PPPPPPPPPP",
    ".kPkkkkPk.",
    "..PkkkP...",
    ".PP..PP...",
    "P......P..",
    ".P....P...",
};

static const char* const k_talon_b[] = {
    "..cc..cc..",
    ".cPkkPPc..",
    "ckPPPPPPk.",
    "kPPyyPPPk.",
    "PPPPPPPPPP",
    ".kPkkkkPk.",
    ".PPkkkPP..",
    "P.P..P.P..",
    ".P....P...",
    "P......P..",
};

static const char* const k_garglet_a[] = {
    "..wwww....",
    ".wGkkGw...",
    "wGGGGGGw..",
    "kGGyyGGk..",
    "GGGGGGGG..",
    ".kGkkGk...",
    ".wG..Gw...",
    "wg....gw..",
    "g......g..",
    ".g....g...",
};

static const char* const k_garglet_b[] = {
    "w......w..",
    ".wGkkGw...",
    "wGGGGGGw..",
    "kGGyyGGk..",
    "GGGGGGGG..",
    ".kGkkGk...",
    ".wG..Gw...",
    "gg....gg..",
    ".g....g...",
    "g......g..",
};

static const char* const k_candle_sprite[] = {
    "...yy...",
    "...yy...",
    "..kwwk..",
    "..kWWk..",
    "..kWWk..",
    "..kggk..",
    "..kkkk..",
    "........",
};

static const char* const k_chalice_sprite[] = {
    "..gggg..",
    "...gg...",
    "..gYYg..",
    "..gYYg..",
    "...gg...",
    "...gg...",
    "..gkkg..",
    "........",
};

static const char* const k_psalter_sprite[] = {
    "..kkkk..",
    "..kBBk..",
    "..kBBk..",
    "..kBBk..",
    "..kYYk..",
    "..kBBk..",
    "..kkkk..",
    "........",
};

static void audio_callback(void* userdata, Uint8* stream, int length) {
  tone_state* state = (tone_state*)userdata;
  int16_t* samples = (int16_t*)stream;
  int count = length / (int)sizeof(int16_t);
  int index = 0;

  for (index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (state->frames_remaining > 0) {
      sample = (state->phase < 3.14159f) ? 1800 : -1800;
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

static int clamp_int(int value, int low, int high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

static int minimum(int left, int right) {
  return left < right ? left : right;
}

static int maximum(int left, int right) {
  return left > right ? left : right;
}

static void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

static void fill_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  int dy = 0;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (dy = -radius; dy <= radius; ++dy) {
    int dx = 0;
    for (dx = -radius; dx <= radius; ++dx) {
      if (dx * dx + dy * dy <= radius * radius) {
        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
      }
    }
  }
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
  static const uint8_t j[7] = {31, 2, 2, 2, 18, 18, 12};
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
  static const uint8_t w[7] = {17, 17, 17, 21, 21, 21, 10};
  static const uint8_t x[7] = {17, 17, 10, 4, 10, 17, 17};
  static const uint8_t y[7] = {17, 17, 10, 4, 4, 4, 4};
  static const uint8_t z[7] = {31, 1, 2, 4, 8, 16, 31};
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
    case 'A': glyph = a; break;
    case 'B': glyph = b; break;
    case 'C': glyph = c; break;
    case 'D': glyph = d; break;
    case 'E': glyph = e; break;
    case 'F': glyph = f; break;
    case 'G': glyph = g; break;
    case 'H': glyph = h; break;
    case 'I': glyph = i; break;
    case 'J': glyph = j; break;
    case 'K': glyph = k; break;
    case 'L': glyph = l; break;
    case 'M': glyph = m; break;
    case 'N': glyph = n; break;
    case 'O': glyph = o; break;
    case 'P': glyph = p; break;
    case 'Q': glyph = q; break;
    case 'R': glyph = r; break;
    case 'S': glyph = s; break;
    case 'T': glyph = t; break;
    case 'U': glyph = u; break;
    case 'V': glyph = v; break;
    case 'W': glyph = w; break;
    case 'X': glyph = x; break;
    case 'Y': glyph = y; break;
    case 'Z': glyph = z; break;
    case '0': glyph = n0; break;
    case '1': glyph = n1; break;
    case '2': glyph = n2; break;
    case '3': glyph = n3; break;
    case '4': glyph = n4; break;
    case '5': glyph = n5; break;
    case '6': glyph = n6; break;
    case '7': glyph = n7; break;
    case '8': glyph = n8; break;
    case '9': glyph = n9; break;
    case ':': glyph = colon; break;
    case '-': glyph = dash; break;
    default: break;
  }
  return glyph[row];
}

static void draw_text(SDL_Renderer* renderer,
                      const char* text,
                      int x,
                      int y,
                      int scale,
                      SDL_Color color,
                      int centered) {
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
        {
          SDL_Rect pixel = {draw_x + column * scale, y + row * scale, scale, scale};
          SDL_RenderFillRect(renderer, &pixel);
        }
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

static const SDL_Color* color_for_key(const palette_entry* palette, int count, char key) {
  int index = 0;
  for (index = 0; index < count; ++index) {
    if (palette[index].key == key) {
      return &palette[index].color;
    }
  }
  return NULL;
}

static void draw_sprite_mask(SDL_Renderer* renderer,
                             const char* const* rows,
                             int row_count,
                             int x,
                             int y,
                             int scale,
                             SDL_Color color,
                             int flip) {
  int row = 0;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (row = 0; row < row_count; ++row) {
    int width = (int)strlen(rows[row]);
    int column = 0;
    for (column = 0; column < width; ++column) {
      int draw_column = flip ? (width - 1 - column) : column;
      if (rows[row][column] == '.') {
        continue;
      }
      {
        SDL_Rect pixel = {x + draw_column * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
  }
}

static void draw_sprite(SDL_Renderer* renderer,
                        const char* const* rows,
                        int row_count,
                        int x,
                        int y,
                        int scale,
                        const palette_entry* palette,
                        int palette_count,
                        int flip) {
  int row = 0;
  for (row = 0; row < row_count; ++row) {
    int width = (int)strlen(rows[row]);
    int column = 0;
    for (column = 0; column < width; ++column) {
      char key = rows[row][column];
      int draw_column = flip ? (width - 1 - column) : column;
      const SDL_Color* color = NULL;
      if (key == '.') {
        continue;
      }
      color = color_for_key(palette, palette_count, key);
      if (color == NULL) {
        continue;
      }
      SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
      {
        SDL_Rect pixel = {x + draw_column * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
  }
}

static void draw_arch_window(SDL_Renderer* renderer,
                             int x,
                             int y,
                             int width,
                             int height,
                             SDL_Color frame,
                             SDL_Color inner,
                             SDL_Color glow) {
  int row = 0;
  SDL_Rect lower = {x, y + 10, width, height - 10};
  fill_rect(renderer, lower, frame);
  for (row = 0; row < 10; ++row) {
    int inset = abs(5 - row) + 1;
    SDL_Rect band = {x + inset, y + row, width - inset * 2, 1};
    fill_rect(renderer, band, frame);
  }

  lower.x = x + 4;
  lower.y = y + 14;
  lower.w = width - 8;
  lower.h = height - 18;
  fill_rect(renderer, lower, inner);
  for (row = 0; row < 7; ++row) {
    int inset = abs(3 - row) + 1;
    SDL_Rect band = {x + 7 + inset, y + 7 + row, width - 14 - inset * 2, 1};
    fill_rect(renderer, band, inner);
  }

  fill_rect(renderer, (SDL_Rect){x + width / 2 - 1, y + 18, 2, height - 26}, glow);
  fill_rect(renderer, (SDL_Rect){x + 10, y + height / 2 - 1, width - 20, 2}, glow);
}

static void draw_twinkle(SDL_Renderer* renderer, int x, int y, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawPoint(renderer, x, y);
  SDL_RenderDrawPoint(renderer, x - 1, y);
  SDL_RenderDrawPoint(renderer, x + 1, y);
  SDL_RenderDrawPoint(renderer, x, y - 1);
  SDL_RenderDrawPoint(renderer, x, y + 1);
}

static int rects_overlap(float ax, float ay, int aw, int ah, float bx, float by, int bw, int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void wrap_x(float* x, int width) {
  if (*x + width < 0.0f) {
    *x = (float)VIRTUAL_WIDTH;
  } else if (*x > (float)VIRTUAL_WIDTH) {
    *x = (float)(-width);
  }
}

static int current_monk_index(const game_state* game) {
  if (game->player.active) {
    return game->player.monk_index;
  }
  return game->next_monk_index;
}

static void save_high_score(const pp_context* context, int high_score) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), BROTHERS_PATH_SEPARATOR);
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
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), BROTHERS_PATH_SEPARATOR);
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

static void award_score(game_state* game, int points) {
  game->score += points;
  if (game->score > game->high_score) {
    game->high_score = game->score;
  }
  while (game->score >= game->next_extra_life) {
    game->lives += 1;
    game->next_extra_life += 20000;
    trigger_tone(&game->tone, 980.0f, 90);
  }
}

static void setup_platforms(game_state* game) {
  int index = 0;
  memset(game->platforms, 0, sizeof(game->platforms));
  for (index = 0; index < MAX_PLATFORMS; ++index) {
    game->platforms[index].rect = k_platform_layout[index];
    game->platforms[index].can_bump = index != MAX_PLATFORMS - 1;
    game->platforms[index].segment_count =
        maximum(1, minimum(MAX_PLATFORM_SEGMENTS, k_platform_layout[index].w / SEGMENT_WIDTH));
  }
}

static void clear_round_entities(game_state* game) {
  int index = 0;
  memset(game->enemies, 0, sizeof(game->enemies));
  memset(game->relics, 0, sizeof(game->relics));
  memset(game->wisps, 0, sizeof(game->wisps));
  memset(game->offerings, 0, sizeof(game->offerings));
  for (index = 0; index < MAX_PLATFORMS; ++index) {
    memset(game->platforms[index].bump_started, 0, sizeof(game->platforms[index].bump_started));
  }
}

static int enemy_score(enemy_kind kind) {
  switch (kind) {
    case ENEMY_IMP:
      return 800;
    case ENEMY_TALON:
      return 1200;
    case ENEMY_GARGLET:
      return 1600;
    default:
      return 500;
  }
}

static float enemy_speed(const enemy* foe) {
  switch (foe->kind) {
    case ENEMY_IMP:
      return 1.05f + 0.18f * foe->speed_tier;
    case ENEMY_TALON:
      return 0.95f + 0.22f * foe->speed_tier + (foe->anger ? 0.35f : 0.0f);
    case ENEMY_GARGLET:
      return 1.10f + 0.15f * foe->speed_tier;
    default:
      return 1.0f;
  }
}

static const char* monk_name(int monk_index) {
  return monk_index == 0 ? "BEDE" : "CASSIAN";
}

static int find_free_enemy_slot(game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_ENEMIES; ++index) {
    if (!game->enemies[index].active) {
      return index;
    }
  }
  return -1;
}

static int find_free_relic_slot(game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_RELICS; ++index) {
    if (!game->relics[index].active) {
      return index;
    }
  }
  return -1;
}

static void spawn_player(game_state* game) {
  player_state* player = &game->player;
  player->active = 1;
  player->x = 246.0f;
  player->y = (float)(FLOOR_Y - PLAYER_H);
  player->vx = 0.0f;
  player->vy = 0.0f;
  player->facing = game->next_monk_index == 0 ? 1 : -1;
  player->on_ground = 1;
  player->platform_index = MAX_PLATFORMS - 1;
  player->monk_index = game->next_monk_index;
  player->invulnerable_until = game->clock + 1500;
  player->respawn_at = 0;
  game->next_monk_index = 1 - game->next_monk_index;
}

static void build_spawn_queue(game_state* game, wave_recipe recipe) {
  int count = 0;
  int index = 0;

  while (recipe.imps > 0 || recipe.talons > 0 || recipe.garglets > 0) {
    if (recipe.imps > 0 && count < MAX_SPAWN_QUEUE) {
      game->spawn_queue[count++] = ENEMY_IMP;
      --recipe.imps;
    }
    if (recipe.talons > 0 && count < MAX_SPAWN_QUEUE) {
      game->spawn_queue[count++] = ENEMY_TALON;
      --recipe.talons;
    }
    if (recipe.garglets > 0 && count < MAX_SPAWN_QUEUE) {
      game->spawn_queue[count++] = ENEMY_GARGLET;
      --recipe.garglets;
    }
  }

  for (index = count - 1; index > 0; --index) {
    int swap_index = rand() % (index + 1);
    enemy_kind temp = game->spawn_queue[index];
    game->spawn_queue[index] = game->spawn_queue[swap_index];
    game->spawn_queue[swap_index] = temp;
  }

  game->spawn_queue_count = count;
  game->spawn_queue_index = 0;
}

static void start_combat_round(game_state* game) {
  wave_recipe recipe = k_wave_recipes[(game->combat_round - 1) % (int)(sizeof(k_wave_recipes) / sizeof(k_wave_recipes[0]))];
  clear_round_entities(game);
  game->bonus_round = 0;
  game->offerings_remaining = 0;
  game->bell_charges = 3;
  game->bell_flash_until = 0;
  game->bell_cooldown_until = 0;
  game->round_started_at = game->clock;
  game->next_spawn_at = game->clock + 850;
  game->next_wisp_at = game->clock + (Uint32)maximum(9000, 15000 - game->combat_round * 450);
  game->banner_until = game->clock + 1700;
  game->clear_until = 0;
  game->perfect_bonus = 0;

  recipe.imps += minimum(2, game->combat_round / 4);
  build_spawn_queue(game, recipe);
  spawn_player(game);
}

static void start_bonus_round(game_state* game) {
  int index = 0;
  clear_round_entities(game);
  game->bonus_round = 1;
  game->offerings_remaining = MAX_OFFERINGS;
  game->bell_charges = 0;
  game->bell_flash_until = 0;
  game->bell_cooldown_until = 0;
  game->round_started_at = game->clock;
  game->next_spawn_at = 0;
  game->next_wisp_at = 0;
  game->banner_until = game->clock + 1700;
  game->clear_until = 0;
  game->perfect_bonus = 1;
  for (index = 0; index < MAX_OFFERINGS; ++index) {
    game->offerings[index].active = 1;
    game->offerings[index].x = (float)k_offering_points[index].x;
    game->offerings[index].y = (float)k_offering_points[index].y;
    game->offerings[index].kind = (relic_kind)(index % 3);
  }
  spawn_player(game);
}

static void start_phase(game_state* game) {
  if (game->phase % 4 == 0) {
    start_bonus_round(game);
  } else {
    game->combat_round += 1;
    start_combat_round(game);
  }
}

static void start_new_game(game_state* game) {
  game->mode = MODE_PLAY;
  game->score = 0;
  game->lives = 3;
  game->phase = 1;
  game->combat_round = 0;
  game->next_extra_life = 20000;
  game->next_monk_index = 0;
  game->player.active = 0;
  game->paused = 0;
  start_phase(game);
}

static int active_enemy_count(const game_state* game) {
  int index = 0;
  int count = 0;
  for (index = 0; index < MAX_ENEMIES; ++index) {
    if (game->enemies[index].active) {
      ++count;
    }
  }
  return count;
}

static int active_relic_count(const game_state* game) {
  int index = 0;
  int count = 0;
  for (index = 0; index < MAX_RELICS; ++index) {
    if (game->relics[index].active) {
      ++count;
    }
  }
  return count;
}

static int active_wisp_count(const game_state* game) {
  int index = 0;
  int count = 0;
  for (index = 0; index < MAX_WISPS; ++index) {
    if (game->wisps[index].active) {
      ++count;
    }
  }
  return count;
}

static void spawn_relic(game_state* game, float x, float y, relic_kind kind, int facing) {
  int slot = find_free_relic_slot(game);
  if (slot < 0) {
    return;
  }
  game->relics[slot].active = 1;
  game->relics[slot].kind = kind;
  game->relics[slot].x = x;
  game->relics[slot].y = y - 8.0f;
  game->relics[slot].vx = 0.25f * (float)facing;
  game->relics[slot].vy = -2.3f;
  game->relics[slot].expire_at = game->clock + 9000;
}

static void banish_enemy(game_state* game, enemy* foe) {
  relic_kind kind = RELIC_CANDLE;
  if (!foe->active) {
    return;
  }
  if (foe->kind == ENEMY_TALON) {
    kind = RELIC_CHALICE;
  } else if (foe->kind == ENEMY_GARGLET) {
    kind = RELIC_PSALTER;
  }
  award_score(game, enemy_score(foe->kind));
  spawn_relic(game, foe->x + ENEMY_W / 2.0f - 4.0f, foe->y, kind, foe->facing);
  foe->active = 0;
  trigger_tone(&game->tone, 960.0f, 70);
}

static void stun_enemy(game_state* game, enemy* foe) {
  foe->state = ENEMY_STUNNED;
  foe->stun_until = game->clock + (Uint32)maximum(1500, 2800 - game->combat_round * 70);
  foe->vy = -2.8f;
  foe->vx = 0.0f;
  foe->on_ground = 0;
  foe->platform_index = -1;
  foe->flash_until = game->clock + 220;
}

static void hit_enemy_with_bump(game_state* game, enemy* foe) {
  if (!foe->active || foe->state == ENEMY_STUNNED) {
    return;
  }
  if (foe->kind == ENEMY_GARGLET && !foe->on_ground) {
    return;
  }
  if (foe->kind == ENEMY_TALON && foe->anger == 0) {
    foe->anger = 1;
    foe->facing *= -1;
    foe->speed_tier = minimum(3, foe->speed_tier + 1);
    foe->flash_until = game->clock + 220;
    trigger_tone(&game->tone, 360.0f, 60);
    return;
  }
  stun_enemy(game, foe);
  trigger_tone(&game->tone, 520.0f, 80);
}

static void trigger_platform_bump(game_state* game, int platform_index, float hit_x) {
  int segment = 0;
  float segment_center = 0.0f;
  int enemy_index = 0;
  platform* ledge = &game->platforms[platform_index];
  segment = clamp_int((int)((hit_x - ledge->rect.x) / SEGMENT_WIDTH), 0, ledge->segment_count - 1);
  segment_center = (float)(ledge->rect.x + segment * SEGMENT_WIDTH + SEGMENT_WIDTH / 2);
  ledge->bump_started[segment] = game->clock;

  for (enemy_index = 0; enemy_index < MAX_ENEMIES; ++enemy_index) {
    enemy* foe = &game->enemies[enemy_index];
    if (!foe->active || foe->platform_index != platform_index || !foe->on_ground) {
      continue;
    }
    if (fabsf((foe->x + ENEMY_W / 2.0f) - segment_center) <= 30.0f) {
      hit_enemy_with_bump(game, foe);
    }
  }
}

static void ring_bell(game_state* game) {
  int index = 0;
  if (game->bell_charges <= 0 || game->clock < game->bell_cooldown_until) {
    trigger_tone(&game->tone, 180.0f, 60);
    return;
  }
  --game->bell_charges;
  game->bell_flash_until = game->clock + 260;
  game->bell_cooldown_until = game->clock + 240;

  for (index = 0; index < MAX_ENEMIES; ++index) {
    enemy* foe = &game->enemies[index];
    if (foe->active && foe->on_ground) {
      hit_enemy_with_bump(game, foe);
    }
  }
  for (index = 0; index < MAX_WISPS; ++index) {
    if (game->wisps[index].active) {
      game->wisps[index].active = 0;
      award_score(game, 150);
    }
  }
  trigger_tone(&game->tone, 260.0f, 150);
}

static int find_landing_platform(const game_state* game,
                                 float x,
                                 int width,
                                 float previous_bottom,
                                 float next_bottom,
                                 float* landing_y) {
  int index = 0;
  int best_index = -1;
  float best_y = 10000.0f;

  for (index = 0; index < MAX_PLATFORMS; ++index) {
    const SDL_Rect rect = game->platforms[index].rect;
    if (previous_bottom > (float)rect.y || next_bottom < (float)rect.y) {
      continue;
    }
    if (x + width <= rect.x + 4 || x >= rect.x + rect.w - 4) {
      continue;
    }
    if ((float)rect.y < best_y) {
      best_y = (float)rect.y;
      best_index = index;
    }
  }

  if (best_index >= 0 && landing_y != NULL) {
    *landing_y = best_y;
  }
  return best_index;
}

static int find_ceiling_platform(const game_state* game,
                                 float x,
                                 int width,
                                 float previous_top,
                                 float next_top,
                                 float* underside_y) {
  int index = 0;
  int best_index = -1;
  float best_underside = -10000.0f;

  for (index = 0; index < MAX_PLATFORMS - 1; ++index) {
    const SDL_Rect rect = game->platforms[index].rect;
    float underside = (float)(rect.y + rect.h);
    if (previous_top < underside || next_top > underside) {
      continue;
    }
    if (x + width <= rect.x + 4 || x >= rect.x + rect.w - 4) {
      continue;
    }
    if (underside > best_underside) {
      best_underside = underside;
      best_index = index;
    }
  }

  if (best_index >= 0 && underside_y != NULL) {
    *underside_y = best_underside;
  }
  return best_index;
}

static void move_player(game_state* game, const frame_input* input) {
  float target_vx = 0.0f;
  float previous_top = 0.0f;
  float previous_bottom = 0.0f;
  float landing_y = 0.0f;
  float underside_y = 0.0f;
  int landing_platform = -1;
  int ceiling_platform = -1;
  player_state* player = &game->player;

  if (!player->active) {
    return;
  }

  if (input->left && !input->right) {
    target_vx = -2.25f;
    player->facing = -1;
  } else if (input->right && !input->left) {
    target_vx = 2.25f;
    player->facing = 1;
  }

  if (target_vx != 0.0f) {
    player->vx += (target_vx - player->vx) * 0.35f;
  } else if (player->on_ground) {
    player->vx *= 0.74f;
  } else {
    player->vx *= 0.94f;
  }
  if (fabsf(player->vx) < 0.05f) {
    player->vx = 0.0f;
  }

  if (input->jump_pressed && player->on_ground) {
    player->vy = -8.75f;
    player->on_ground = 0;
    player->platform_index = -1;
    trigger_tone(&game->tone, 720.0f, 55);
  }

  player->x += player->vx;
  wrap_x(&player->x, PLAYER_W);

  previous_top = player->y;
  previous_bottom = player->y + PLAYER_H;
  player->vy += 0.32f;
  if (player->vy > 7.4f) {
    player->vy = 7.4f;
  }
  player->y += player->vy;
  player->on_ground = 0;
  player->platform_index = -1;

  landing_platform = find_landing_platform(game, player->x, PLAYER_W, previous_bottom,
                                           player->y + PLAYER_H, &landing_y);
  if (landing_platform >= 0 && player->vy >= 0.0f) {
    player->y = landing_y - PLAYER_H;
    player->vy = 0.0f;
    player->on_ground = 1;
    player->platform_index = landing_platform;
  } else if (player->vy < 0.0f) {
    float bell_underside = (float)(BELL_Y + BELL_H);
    if (previous_top >= bell_underside && player->y <= bell_underside && player->x + PLAYER_W > BELL_X + 3 &&
        player->x < BELL_X + BELL_W - 3) {
      player->y = bell_underside;
      player->vy = 1.5f;
      ring_bell(game);
    } else {
      ceiling_platform = find_ceiling_platform(game, player->x, PLAYER_W, previous_top, player->y, &underside_y);
      if (ceiling_platform >= 0) {
        player->y = underside_y;
        player->vy = 1.3f;
        trigger_platform_bump(game, ceiling_platform, player->x + PLAYER_W / 2.0f);
      }
    }
  }
}

static void move_enemy(game_state* game, enemy* foe) {
  float previous_bottom = foe->y + ENEMY_H;
  float landing_y = 0.0f;
  int landing_platform = -1;

  if (foe->state == ENEMY_STUNNED) {
    foe->vy += 0.30f;
    if (foe->vy > 6.5f) {
      foe->vy = 6.5f;
    }
    foe->x += foe->vx;
    foe->vx *= 0.85f;
    wrap_x(&foe->x, ENEMY_W);
    foe->y += foe->vy;
    foe->on_ground = 0;
    foe->platform_index = -1;
    landing_platform =
        find_landing_platform(game, foe->x, ENEMY_W, previous_bottom, foe->y + ENEMY_H, &landing_y);
    if (landing_platform >= 0 && foe->vy >= 0.0f) {
      foe->y = landing_y - ENEMY_H;
      foe->vy = 0.0f;
      foe->on_ground = 1;
      foe->platform_index = landing_platform;
    }
    if (game->clock >= foe->stun_until) {
      foe->state = ENEMY_NORMAL;
      foe->speed_tier = minimum(3, foe->speed_tier + 1);
      if (foe->kind == ENEMY_TALON) {
        foe->anger = 1;
      }
      foe->flash_until = game->clock + 200;
    }
    return;
  }

  if (foe->on_ground) {
    foe->vx = enemy_speed(foe) * (float)foe->facing;
    if (foe->kind == ENEMY_GARGLET && game->clock >= foe->next_hop_at) {
      foe->vy = -6.2f;
      foe->on_ground = 0;
      foe->platform_index = -1;
      foe->next_hop_at = game->clock + (Uint32)maximum(650, 1080 - game->combat_round * 25);
    }
  }

  previous_bottom = foe->y + ENEMY_H;
  foe->x += foe->vx;
  wrap_x(&foe->x, ENEMY_W);
  foe->vy += 0.28f;
  if (foe->vy > 6.8f) {
    foe->vy = 6.8f;
  }
  foe->y += foe->vy;
  foe->on_ground = 0;
  foe->platform_index = -1;

  landing_platform = find_landing_platform(game, foe->x, ENEMY_W, previous_bottom, foe->y + ENEMY_H, &landing_y);
  if (landing_platform >= 0 && foe->vy >= 0.0f) {
    foe->y = landing_y - ENEMY_H;
    foe->vy = 0.0f;
    foe->on_ground = 1;
    foe->platform_index = landing_platform;
    if (foe->kind == ENEMY_GARGLET && foe->next_hop_at < game->clock + 200) {
      foe->next_hop_at = game->clock + (Uint32)maximum(650, 1020 - game->combat_round * 20);
    }
  }
}

static void spawn_enemy(game_state* game) {
  int slot = 0;
  enemy* foe = NULL;
  int spawn_left = 0;

  if (game->spawn_queue_index >= game->spawn_queue_count) {
    return;
  }

  slot = find_free_enemy_slot(game);
  if (slot < 0) {
    game->next_spawn_at = game->clock + 400;
    return;
  }

  foe = &game->enemies[slot];
  memset(foe, 0, sizeof(*foe));
  spawn_left = (game->spawn_queue_index % 2) == 0;

  foe->active = 1;
  foe->kind = game->spawn_queue[game->spawn_queue_index++];
  foe->state = ENEMY_NORMAL;
  foe->x = spawn_left ? 56.0f : 436.0f;
  foe->y = 28.0f;
  foe->vx = 0.0f;
  foe->vy = 0.0f;
  foe->facing = spawn_left ? 1 : -1;
  foe->on_ground = 0;
  foe->platform_index = -1;
  foe->anger = 0;
  foe->speed_tier = minimum(2, (game->combat_round - 1) / 3);
  foe->stun_until = 0;
  foe->flash_until = game->clock + 180;
  foe->next_hop_at = game->clock + 900;
  game->next_spawn_at = game->clock + (Uint32)maximum(700, 1400 - game->combat_round * 45);
}

static void spawn_wisp(game_state* game) {
  static const int k_lanes[] = {146, 218, 290, 362};
  int index = 0;
  int slot = -1;
  int limit = minimum(MAX_WISPS, 1 + game->combat_round / 4);

  if (active_wisp_count(game) >= limit) {
    game->next_wisp_at = game->clock + 3200;
    return;
  }
  for (index = 0; index < MAX_WISPS; ++index) {
    if (!game->wisps[index].active) {
      slot = index;
      break;
    }
  }
  if (slot < 0) {
    return;
  }

  game->wisps[slot].active = 1;
  game->wisps[slot].born_at = game->clock;
  game->wisps[slot].base_y = (float)k_lanes[rand() % 4];
  if ((rand() & 1) == 0) {
    game->wisps[slot].x = -20.0f;
    game->wisps[slot].vx = 1.6f + 0.08f * game->combat_round;
  } else {
    game->wisps[slot].x = (float)VIRTUAL_WIDTH + 20.0f;
    game->wisps[slot].vx = -1.6f - 0.08f * game->combat_round;
  }
  game->next_wisp_at = game->clock + (Uint32)maximum(4200, 7600 - game->combat_round * 120);
}

static void update_enemies(game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_ENEMIES; ++index) {
    if (game->enemies[index].active) {
      move_enemy(game, &game->enemies[index]);
    }
  }

  for (index = 0; index < MAX_ENEMIES; ++index) {
    int other = 0;
    enemy* left = &game->enemies[index];
    if (!left->active || left->state != ENEMY_NORMAL) {
      continue;
    }
    for (other = index + 1; other < MAX_ENEMIES; ++other) {
      enemy* right = &game->enemies[other];
      if (!right->active || right->state != ENEMY_NORMAL) {
        continue;
      }
      if (fabsf(left->y - right->y) > 10.0f) {
        continue;
      }
      if (rects_overlap(left->x, left->y, ENEMY_W, ENEMY_H, right->x, right->y, ENEMY_W, ENEMY_H)) {
        left->facing *= -1;
        right->facing *= -1;
      }
    }
  }
}

static void update_relics(game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_RELICS; ++index) {
    relic* token = &game->relics[index];
    if (!token->active) {
      continue;
    }
    if (game->clock >= token->expire_at) {
      token->active = 0;
      continue;
    }
    token->x += token->vx;
    token->vy += 0.10f;
    if (token->vy > 1.7f) {
      token->vy = 1.7f;
    }
    token->y += token->vy;
    if (token->y > 428.0f) {
      token->y = 428.0f;
      token->vy *= -0.35f;
      if (fabsf(token->vy) < 0.18f) {
        token->vy = 0.0f;
      }
    }
  }
}

static void update_wisps(game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_WISPS; ++index) {
    if (!game->wisps[index].active) {
      continue;
    }
    game->wisps[index].x += game->wisps[index].vx;
    if (game->wisps[index].vx > 0.0f && game->wisps[index].x > (float)VIRTUAL_WIDTH + 24.0f) {
      game->wisps[index].x = -24.0f;
    } else if (game->wisps[index].vx < 0.0f && game->wisps[index].x < -24.0f) {
      game->wisps[index].x = (float)VIRTUAL_WIDTH + 24.0f;
    }
  }
}

static void kill_player(game_state* game) {
  if (!game->player.active || game->mode != MODE_PLAY) {
    return;
  }
  game->player.active = 0;
  game->player.vx = 0.0f;
  game->player.vy = 0.0f;
  game->player.respawn_at = game->clock + 1100;
  game->lives -= 1;
  trigger_tone(&game->tone, 150.0f, 220);
  if (game->lives <= 0) {
    game->mode = MODE_GAME_OVER;
  }
}

static void resolve_bonus_collects(game_state* game) {
  int index = 0;
  if (!game->player.active) {
    return;
  }
  for (index = 0; index < MAX_OFFERINGS; ++index) {
    offering* token = &game->offerings[index];
    if (!token->active) {
      continue;
    }
    if (rects_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H, token->x - 8.0f, token->y - 8.0f, 16, 16)) {
      token->active = 0;
      --game->offerings_remaining;
      award_score(game, 500);
      trigger_tone(&game->tone, 1040.0f, 45);
    }
  }
}

static void resolve_play_collisions(game_state* game) {
  int index = 0;
  if (!game->player.active) {
    return;
  }

  for (index = 0; index < MAX_ENEMIES; ++index) {
    enemy* foe = &game->enemies[index];
    if (!foe->active) {
      continue;
    }
    if (!rects_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H, foe->x, foe->y, ENEMY_W, ENEMY_H)) {
      continue;
    }
    if (foe->state == ENEMY_STUNNED) {
      banish_enemy(game, foe);
      continue;
    }
    if (game->clock >= game->player.invulnerable_until) {
      kill_player(game);
      return;
    }
  }

  for (index = 0; index < MAX_RELICS; ++index) {
    relic* token = &game->relics[index];
    if (!token->active) {
      continue;
    }
    if (rects_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H, token->x, token->y, 12, 12)) {
      int points = 400;
      if (token->kind == RELIC_CHALICE) {
        points = 650;
      } else if (token->kind == RELIC_PSALTER) {
        points = 900;
      }
      token->active = 0;
      award_score(game, points);
      trigger_tone(&game->tone, 1180.0f, 50);
    }
  }

  for (index = 0; index < MAX_WISPS; ++index) {
    float y = 0.0f;
    if (!game->wisps[index].active) {
      continue;
    }
    y = game->wisps[index].base_y + sinf((float)(game->clock - game->wisps[index].born_at) / 140.0f) * 4.0f;
    if (rects_overlap(game->player.x, game->player.y, PLAYER_W, PLAYER_H, game->wisps[index].x - 8.0f, y - 8.0f, 16, 16) &&
        game->clock >= game->player.invulnerable_until) {
      kill_player(game);
      return;
    }
  }
}

static void update_play_state(game_state* game, const frame_input* input) {
  if (game->paused) {
    (void)input;
    return;
  }

  if (game->clear_until != 0) {
    if (game->clock >= game->clear_until) {
      game->phase += 1;
      start_phase(game);
    }
    return;
  }

  if (!game->player.active) {
    if (game->mode == MODE_PLAY && game->lives > 0 && game->player.respawn_at != 0 && game->clock >= game->player.respawn_at) {
      spawn_player(game);
    }
  } else {
    move_player(game, input);
  }

  if (game->bonus_round) {
    resolve_bonus_collects(game);
    if (game->offerings_remaining <= 0) {
      award_score(game, 2500);
      trigger_tone(&game->tone, 860.0f, 150);
      game->clear_until = game->clock + 1600;
    } else if (game->clock - game->round_started_at >= 18000) {
      game->perfect_bonus = 0;
      game->clear_until = game->clock + 1400;
    }
    return;
  }

  if (game->spawn_queue_index < game->spawn_queue_count && game->clock >= game->next_spawn_at) {
    spawn_enemy(game);
  }
  if (game->clock >= game->next_wisp_at) {
    spawn_wisp(game);
  }

  update_enemies(game);
  update_relics(game);
  update_wisps(game);
  resolve_play_collisions(game);

  if (game->spawn_queue_index >= game->spawn_queue_count && active_enemy_count(game) == 0 && active_relic_count(game) == 0 &&
      active_wisp_count(game) == 0) {
    award_score(game, 1000 + game->phase * 150);
    trigger_tone(&game->tone, 680.0f, 140);
    game->clear_until = game->clock + 1600;
  }
}

static int bell_is_flashing(const game_state* game) {
  return game->clock < game->bell_flash_until;
}

static int enemy_is_flashing(const enemy* foe, Uint32 now) {
  return now < foe->flash_until && ((now / 70) % 2 == 0);
}

static void draw_monk_pose(SDL_Renderer* renderer,
                           int monk_index,
                           int pose,
                           int x,
                           int y,
                           int scale,
                           int flip,
                           int flashing) {
  SDL_Color robe_dark = monk_index == 0 ? (SDL_Color){58, 42, 27, 255} : (SDL_Color){40, 53, 76, 255};
  SDL_Color robe_light = monk_index == 0 ? (SDL_Color){120, 94, 70, 255} : (SDL_Color){93, 113, 152, 255};
  SDL_Color accent = monk_index == 0 ? (SDL_Color){191, 90, 74, 255} : (SDL_Color){96, 186, 184, 255};
  SDL_Color beads = monk_index == 0 ? (SDL_Color){224, 193, 139, 255} : (SDL_Color){208, 223, 245, 255};
  SDL_Color outline = flashing ? k_glow : (SDL_Color){16, 10, 12, 255};
  SDL_Color skin = {236, 206, 182, 255};
  SDL_Color tonsure = {136, 96, 61, 255};
  palette_entry palette[] = {
      {'k', outline}, {'s', skin}, {'t', tonsure}, {'R', robe_light}, {'r', robe_dark}, {'c', accent}, {'b', beads},
  };
  const char* const* sprite = k_monk_idle;
  if (pose == 1) {
    sprite = k_monk_stride;
  } else if (pose == 2) {
    sprite = k_monk_jump;
  }
  draw_sprite_mask(renderer, sprite, 12, x + scale, y + scale, scale, k_shadow, flip);
  draw_sprite(renderer, sprite, 12, x, y, scale, palette, (int)(sizeof(palette) / sizeof(palette[0])), flip);
}

static void draw_enemy_sprite(SDL_Renderer* renderer, const enemy* foe, Uint32 now) {
  palette_entry palette[6];
  const char* const* sprite = k_imp_a;
  int sprite_count = 10;
  int alt = (int)((now / 180) % 2);
  SDL_Color outline = enemy_is_flashing(foe, now) ? k_glow : (SDL_Color){16, 10, 12, 255};

  if (foe->kind == ENEMY_IMP) {
    sprite = alt ? k_imp_b : k_imp_a;
    palette[0] = (palette_entry){'k', outline};
    palette[1] = (palette_entry){'R', foe->state == ENEMY_STUNNED ? k_dull : (SDL_Color){217, 90, 76, 255}};
    palette[2] = (palette_entry){'r', foe->state == ENEMY_STUNNED ? k_muted : (SDL_Color){130, 33, 40, 255}};
    palette[3] = (palette_entry){'h', foe->state == ENEMY_STUNNED ? k_muted : (SDL_Color){235, 212, 183, 255}};
    palette[4] = (palette_entry){'y', k_glow};
  } else if (foe->kind == ENEMY_TALON) {
    sprite = alt ? k_talon_b : k_talon_a;
    palette[0] = (palette_entry){'k', outline};
    palette[1] = (palette_entry){'P', foe->state == ENEMY_STUNNED ? k_dull : (SDL_Color){155, 100, 183, 255}};
    palette[2] = (palette_entry){'c', foe->state == ENEMY_STUNNED ? k_muted : (SDL_Color){232, 213, 187, 255}};
    palette[3] = (palette_entry){'y', k_glow};
    palette[4] = (palette_entry){'r', k_bg_top};
  } else {
    sprite = alt ? k_garglet_b : k_garglet_a;
    palette[0] = (palette_entry){'k', outline};
    palette[1] = (palette_entry){'G', foe->state == ENEMY_STUNNED ? k_dull : (SDL_Color){105, 191, 146, 255}};
    palette[2] = (palette_entry){'g', foe->state == ENEMY_STUNNED ? k_muted : (SDL_Color){42, 112, 82, 255}};
    palette[3] = (palette_entry){'w', foe->state == ENEMY_STUNNED ? k_muted : (SDL_Color){198, 227, 221, 255}};
    palette[4] = (palette_entry){'y', k_glow};
  }

  draw_sprite_mask(renderer, sprite, sprite_count, (int)foe->x + 2, (int)foe->y + 2, 2, k_shadow, foe->facing < 0);
  draw_sprite(renderer, sprite, sprite_count, (int)foe->x, (int)foe->y, 2, palette, 5, foe->facing < 0);
  if (foe->state == ENEMY_STUNNED) {
    draw_twinkle(renderer, (int)foe->x + 8, (int)foe->y - 2, k_glow);
    draw_twinkle(renderer, (int)foe->x + 16, (int)foe->y + 1, k_glow);
  }
}

static void draw_relic_sprite(SDL_Renderer* renderer, relic_kind kind, int x, int y, int scale) {
  palette_entry palette[] = {
      {'k', (SDL_Color){22, 16, 20, 255}},
      {'g', k_gold_dark},
      {'Y', k_glow},
      {'y', k_glow},
      {'w', (SDL_Color){244, 238, 224, 255}},
      {'W', (SDL_Color){220, 208, 177, 255}},
      {'B', (SDL_Color){108, 137, 197, 255}},
  };
  const char* const* sprite = k_candle_sprite;
  if (kind == RELIC_CHALICE) {
    sprite = k_chalice_sprite;
  } else if (kind == RELIC_PSALTER) {
    sprite = k_psalter_sprite;
  }
  draw_sprite_mask(renderer, sprite, 8, x + scale, y + scale, scale, k_shadow, 0);
  draw_sprite(renderer, sprite, 8, x, y, scale, palette, (int)(sizeof(palette) / sizeof(palette[0])), 0);
}

static int platform_bump_offset(const platform* ledge, int segment, Uint32 now) {
  Uint32 started = ledge->bump_started[segment];
  Uint32 elapsed = 0;
  if (started == 0 || now <= started) {
    return 0;
  }
  elapsed = now - started;
  if (elapsed >= 160) {
    return 0;
  }
  if (elapsed < 70) {
    return -(int)(elapsed / 10) - 1;
  }
  return -(int)((160 - elapsed) / 18);
}

static void draw_backdrop(SDL_Renderer* renderer, const game_state* game) {
  int y = 0;
  for (y = 0; y < VIRTUAL_HEIGHT; y += 4) {
    SDL_Color band = {
        (Uint8)(k_bg_top.r + ((k_bg_bottom.r - k_bg_top.r) * y) / VIRTUAL_HEIGHT),
        (Uint8)(k_bg_top.g + ((k_bg_bottom.g - k_bg_top.g) * y) / VIRTUAL_HEIGHT),
        (Uint8)(k_bg_top.b + ((k_bg_bottom.b - k_bg_top.b) * y) / VIRTUAL_HEIGHT),
        255,
    };
    fill_rect(renderer, (SDL_Rect){0, y, VIRTUAL_WIDTH, 4}, band);
  }

  fill_rect(renderer, (SDL_Rect){0, 72, VIRTUAL_WIDTH, VIRTUAL_HEIGHT - 72}, k_wall);
  fill_rect(renderer, (SDL_Rect){0, 72, 20, VIRTUAL_HEIGHT - 72}, k_wall_shadow);
  fill_rect(renderer, (SDL_Rect){VIRTUAL_WIDTH - 20, 72, 20, VIRTUAL_HEIGHT - 72}, k_wall_shadow);

  draw_arch_window(renderer, 56, 88, 68, 88, k_wall_shadow, k_window, k_window_glow);
  draw_arch_window(renderer, 222, 56, 68, 96, k_wall_shadow, k_window_red, k_glow);
  draw_arch_window(renderer, 388, 88, 68, 88, k_wall_shadow, k_window_gold, k_glow);

  fill_rect(renderer, (SDL_Rect){60, 74, 22, 90}, k_wall_shadow);
  fill_rect(renderer, (SDL_Rect){64, 78, 14, 82}, k_stone_shadow);
  fill_rect(renderer, (SDL_Rect){430, 74, 22, 90}, k_wall_shadow);
  fill_rect(renderer, (SDL_Rect){434, 78, 14, 82}, k_stone_shadow);
  fill_circle(renderer, 71, 94, 8, k_gold_dark);
  fill_circle(renderer, 71, 94, 4, k_glow);
  fill_rect(renderer, (SDL_Rect){68, 101, 6, 20}, k_stone_mid);
  fill_circle(renderer, 441, 94, 8, k_gold_dark);
  fill_circle(renderer, 441, 94, 4, k_glow);
  fill_rect(renderer, (SDL_Rect){438, 101, 6, 20}, k_stone_mid);

  fill_circle(renderer, 256, 52, 20, (SDL_Color){32, 41, 67, 255});
  fill_circle(renderer, 256, 52, 13, k_window_glow);
  fill_rect(renderer, (SDL_Rect){254, 37, 4, 30}, k_wall_shadow);
  fill_rect(renderer, (SDL_Rect){241, 50, 30, 4}, k_wall_shadow);

  for (y = 94; y < VIRTUAL_HEIGHT; y += 28) {
    fill_rect(renderer, (SDL_Rect){24, y, 464, 2}, (SDL_Color){40, 46, 68, 255});
  }
  for (y = 40; y < 72; y += 10) {
    draw_twinkle(renderer, 76 + (y % 28), y, k_glow);
    draw_twinkle(renderer, 412 - (y % 24), y + 6, k_glow);
  }
  fill_rect(renderer, (SDL_Rect){0, FLOOR_Y + PLATFORM_HEIGHT, VIRTUAL_WIDTH, 56}, k_floor_shadow);
  (void)game;
}

static void draw_platforms(SDL_Renderer* renderer, const game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_PLATFORMS; ++index) {
    const platform* ledge = &game->platforms[index];
    int segment = 0;
    int segments = ledge->segment_count;
    int segment_width = ledge->rect.w / segments;
    for (segment = 0; segment < segments; ++segment) {
      int bump = platform_bump_offset(ledge, segment, game->clock);
      SDL_Rect body = {ledge->rect.x + segment * segment_width, ledge->rect.y + bump, segment_width, ledge->rect.h};
      fill_rect(renderer, body, k_stone_mid);
      fill_rect(renderer, (SDL_Rect){body.x, body.y, body.w, 3}, k_stone_top);
      fill_rect(renderer, (SDL_Rect){body.x, body.y + body.h - 3, body.w, 3}, k_stone_shadow);
      stroke_rect(renderer, body, k_wall_shadow);
      fill_rect(renderer, (SDL_Rect){body.x + 4, body.y + 5, 6, 2}, k_stone_shadow);
      fill_rect(renderer, (SDL_Rect){body.x + body.w - 10, body.y + 7, 4, 2}, k_stone_top);
    }
  }
}

static void draw_bell_block(SDL_Renderer* renderer, const game_state* game) {
  SDL_Color fill = game->bell_charges > 0 ? k_gold : k_dull;
  SDL_Color shine = bell_is_flashing(game) ? k_glow : k_stone_top;
  fill_rect(renderer, (SDL_Rect){BELL_X - 2, BELL_Y - 2, BELL_W + 4, BELL_H + 4}, k_wall_shadow);
  fill_rect(renderer, (SDL_Rect){BELL_X + 12, BELL_Y + 2, 8, 24}, fill);
  fill_rect(renderer, (SDL_Rect){BELL_X + 5, BELL_Y + 8, 22, 8}, fill);
  fill_rect(renderer, (SDL_Rect){BELL_X + 14, BELL_Y + 4, 4, 20}, k_gold_dark);
  fill_rect(renderer, (SDL_Rect){BELL_X + 7, BELL_Y + 10, 18, 4}, k_gold_dark);
  fill_rect(renderer, (SDL_Rect){BELL_X + 13, BELL_Y + 5, 2, 18}, shine);
  fill_rect(renderer, (SDL_Rect){BELL_X + 8, BELL_Y + 11, 16, 2}, shine);
  fill_circle(renderer, BELL_X + BELL_W / 2, BELL_Y + 12, 3, shine);
  if (bell_is_flashing(game)) {
    draw_twinkle(renderer, BELL_X - 6, BELL_Y + 8, k_glow);
    draw_twinkle(renderer, BELL_X + BELL_W + 6, BELL_Y + 8, k_glow);
    draw_twinkle(renderer, BELL_X + BELL_W / 2, BELL_Y - 8, k_glow);
  }
}

static void draw_wisp(SDL_Renderer* renderer, const wisp* spirit, Uint32 now) {
  int x = (int)spirit->x;
  int y = (int)(spirit->base_y + sinf((float)(now - spirit->born_at) / 140.0f) * 4.0f);
  fill_circle(renderer, x, y, 7, k_wisp_outer);
  fill_circle(renderer, x, y, 4, k_wisp_core);
  fill_rect(renderer, (SDL_Rect){x - 2, y + 5, 4, 6}, k_wisp_outer);
  draw_twinkle(renderer, x + 8, y - 2, k_glow);
}

static void draw_hud(SDL_Renderer* renderer, const game_state* game) {
  char buffer[64];
  fill_rect(renderer, (SDL_Rect){0, 0, VIRTUAL_WIDTH, 56}, (SDL_Color){11, 10, 20, 255});
  fill_rect(renderer, (SDL_Rect){0, 56, VIRTUAL_WIDTH, 4}, k_gold_dark);

  draw_text(renderer, "1UP", 18, 12, 2, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%06d", game->score);
  draw_text(renderer, buffer, 18, 28, 2, k_text, 0);

  draw_text(renderer, "HIGH", 184, 12, 2, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%06d", game->high_score);
  draw_text(renderer, buffer, 184, 28, 2, k_text, 0);

  draw_text(renderer, "PHASE", 356, 12, 2, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%02d", game->phase);
  draw_text(renderer, buffer, 432, 28, 2, k_text, 0);

  draw_text(renderer, monk_name(current_monk_index(game)), 18, 42, 1, k_glow, 0);
  draw_text(renderer, "LIVES", 358, 42, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%d", game->lives);
  draw_text(renderer, buffer, 408, 42, 1, k_life, 0);

  fill_rect(renderer, (SDL_Rect){448, 38, 48, 10}, k_wall_shadow);
  stroke_rect(renderer, (SDL_Rect){448, 38, 48, 10}, k_stone_shadow);
  snprintf(buffer, sizeof(buffer), "%d", game->bell_charges);
  draw_text_right(renderer, buffer, 494, 40, 1, game->bell_charges > 0 ? k_glow : k_muted);
  draw_text(renderer, "CROSS", 448, 40, 1, k_muted, 0);
}

static void draw_overlay_box(SDL_Renderer* renderer, const char* title, const char* subtitle, const char* prompt) {
  fill_rect(renderer, (SDL_Rect){64, 172, 384, 138}, k_overlay);
  stroke_rect(renderer, (SDL_Rect){64, 172, 384, 138}, k_gold_dark);
  fill_rect(renderer, (SDL_Rect){76, 184, 360, 18}, k_gold_dark);
  draw_text(renderer, title, 256, 194, 3, k_text, 1);
  if (subtitle != NULL && subtitle[0] != '\0') {
    draw_text(renderer, subtitle, 256, 228, 2, k_muted, 1);
  }
  if (prompt != NULL && prompt[0] != '\0') {
    draw_text(renderer, prompt, 256, 274, 2, k_glow, 1);
  }
}

static void draw_title_screen(SDL_Renderer* renderer, const game_state* game) {
  draw_backdrop(renderer, game);
  draw_platforms(renderer, game);
  draw_bell_block(renderer, game);

  draw_text(renderer, "BROTHERS", 256, 84, 4, k_text, 1);
  draw_text(renderer, "UNDERCROFT VIGIL", 256, 122, 2, k_glow, 1);

  draw_monk_pose(renderer, 0, 1, 104, 240, 5, 0, 0);
  draw_monk_pose(renderer, 1, 0, 296, 238, 5, 1, 0);

  {
    enemy imp = {1, ENEMY_IMP, ENEMY_NORMAL, 222.0f, 292.0f, 0.0f, 0.0f, 1, 1, 4, 0, 0, 0, 0, 0};
    enemy garglet = {1, ENEMY_GARGLET, ENEMY_NORMAL, 262.0f, 292.0f, 0.0f, 0.0f, -1, 1, 5, 0, 0, 0, 0, 0};
    draw_enemy_sprite(renderer, &imp, game->clock);
    draw_enemy_sprite(renderer, &garglet, game->clock);
  }

  draw_overlay_box(renderer, "RUN JUMP BUMP BANISH", "A OR B TO LEAP    START TO BEGIN",
                   "CLASSIC SINGLE SCREEN ARCADE FOR PIXELPAL");
}

static void draw_bonus_offerings(SDL_Renderer* renderer, const game_state* game) {
  int index = 0;
  for (index = 0; index < MAX_OFFERINGS; ++index) {
    if (game->offerings[index].active) {
      int bob = (int)(sinf((float)(game->clock + index * 41) / 180.0f) * 3.0f);
      draw_relic_sprite(renderer, game->offerings[index].kind, (int)game->offerings[index].x - 8,
                        (int)game->offerings[index].y - 8 + bob, 2);
      draw_twinkle(renderer, (int)game->offerings[index].x + 6, (int)game->offerings[index].y - 10 + bob, k_glow);
    }
  }
}

static void draw_play_scene(SDL_Renderer* renderer, const game_state* game) {
  int index = 0;
  draw_backdrop(renderer, game);
  draw_platforms(renderer, game);
  draw_bell_block(renderer, game);
  draw_hud(renderer, game);

  for (index = 0; index < MAX_WISPS; ++index) {
    if (game->wisps[index].active) {
      draw_wisp(renderer, &game->wisps[index], game->clock);
    }
  }
  for (index = 0; index < MAX_RELICS; ++index) {
    if (game->relics[index].active) {
      int bob = (int)(sinf((float)(game->clock + index * 29) / 150.0f) * 2.0f);
      draw_relic_sprite(renderer, game->relics[index].kind, (int)game->relics[index].x, (int)game->relics[index].y + bob, 2);
    }
  }
  if (game->bonus_round) {
    draw_bonus_offerings(renderer, game);
  }
  for (index = 0; index < MAX_ENEMIES; ++index) {
    if (game->enemies[index].active) {
      draw_enemy_sprite(renderer, &game->enemies[index], game->clock);
    }
  }

  if (game->player.active) {
    int pose = 0;
    if (!game->player.on_ground) {
      pose = 2;
    } else if (fabsf(game->player.vx) > 0.35f) {
      pose = (game->clock / 140) % 2;
    }
    draw_monk_pose(renderer, game->player.monk_index, pose, (int)game->player.x, (int)game->player.y, 2,
                   game->player.facing < 0, game->clock < game->player.invulnerable_until && ((game->clock / 80) % 2 == 0));
  }

  if (game->banner_until > game->clock) {
    if (game->bonus_round) {
      draw_overlay_box(renderer, "OFFERING ROUND", "GATHER EVERY RELIC BEFORE THE CANDLES DIM", "");
    } else {
      char phase_text[32];
      snprintf(phase_text, sizeof(phase_text), "PHASE %02d", game->phase);
      draw_overlay_box(renderer, phase_text, monk_name(current_monk_index(game)), "");
    }
  } else if (game->clear_until > game->clock) {
    if (game->bonus_round && game->perfect_bonus) {
      draw_overlay_box(renderer, "PERFECT OFFERING", "THE CLOISTER IS PLEASED", "");
    } else {
      draw_overlay_box(renderer, "CRYPT CLEARED", "DESCEND DEEPER", "");
    }
  } else if (!game->player.active && game->mode == MODE_PLAY && game->lives > 0) {
    draw_overlay_box(renderer, "BROTHER FALLEN", "ANOTHER MONK DESCENDS", "");
  } else if (game->paused) {
    draw_overlay_box(renderer, "PAUSED", "PRESS START TO RESUME", "");
  }
}

static void render_scene(SDL_Renderer* renderer, const game_state* game) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  if (game->mode == MODE_TITLE) {
    draw_title_screen(renderer, game);
  } else {
    draw_play_scene(renderer, game);
    if (game->mode == MODE_GAME_OVER) {
      draw_overlay_box(renderer, "THE CRYPT WINS", "PRESS A B OR START", "HIGH SCORE SAVED");
    }
  }
  SDL_RenderPresent(renderer);
}

int main(int argc, char** argv) {
  pp_context context;
  pp_input_state input;
  pp_input_state previous_input;
  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  SDL_AudioDeviceID audio_device = 0;
  pp_audio_spec audio_spec;
  game_state game;
  Uint32 last_tick = 0;
  Uint32 accumulator = 0;
  int width = 0;
  int height = 0;

  (void)argc;
  (void)argv;

  memset(&game, 0, sizeof(game));
  memset(&previous_input, 0, sizeof(previous_input));
  srand((unsigned int)time(NULL));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (pp_init(&context, "brothers") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  setup_platforms(&game);
  game.high_score = load_high_score(&context);
  game.mode = MODE_TITLE;
  game.next_monk_index = 0;

  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(512, width);
  height = maximum(512, height);

  window = SDL_CreateWindow("BROTHERS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
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
  audio_spec.userdata = &game.tone;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  last_tick = SDL_GetTicks();
  while (!pp_should_exit(&context)) {
    Uint32 now = SDL_GetTicks();
    Uint32 frame_time = now - last_tick;
    frame_input frame = {0, 0, 0, 0, 0};

    if (frame_time > 100) {
      frame_time = 100;
    }
    last_tick = now;
    accumulator += frame_time;

    pp_poll_input(&context, &input);
    frame.left = input.left;
    frame.right = input.right;
    frame.jump_held = input.a || input.b || input.up;
    frame.jump_pressed = (input.a || input.b || input.up) && !(previous_input.a || previous_input.b || previous_input.up);
    frame.start_pressed = input.start && !previous_input.start;

    if (game.mode == MODE_TITLE && (frame.jump_pressed || frame.start_pressed)) {
      start_new_game(&game);
    } else if (game.mode == MODE_GAME_OVER && (frame.jump_pressed || frame.start_pressed)) {
      start_new_game(&game);
    } else if (game.mode == MODE_PLAY && frame.start_pressed) {
      game.paused = !game.paused;
      trigger_tone(&game.tone, game.paused ? 320.0f : 640.0f, 55);
    }

    while (accumulator >= STEP_MS) {
      accumulator -= STEP_MS;
      game.clock += STEP_MS;
      if (game.mode == MODE_PLAY) {
        update_play_state(&game, &frame);
      }
    }

    render_scene(renderer, &game);
    previous_input = input;
    SDL_Delay(1);
  }

  save_high_score(&context, game.high_score);
  if (audio_device != 0U) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
