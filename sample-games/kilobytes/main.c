#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define KILOBYTES_PATH_SEPARATOR '\\'
#else
#define KILOBYTES_PATH_SEPARATOR '/'
#endif

#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512
#define GRID_SIZE 4
#define TILE_SIZE 60
#define TILE_GAP 10
#define BOARD_X 28
#define BOARD_Y 118
#define BOARD_W (GRID_SIZE * TILE_SIZE + (GRID_SIZE + 1) * TILE_GAP)
#define BOARD_H BOARD_W
#define PANEL_X 346
#define PANEL_Y 118
#define PANEL_W 136
#define PANEL_H 286
#define GOAL_TILE 2048

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

typedef struct game_state {
  int board[GRID_SIZE][GRID_SIZE];
  int score;
  int best_score;
  int best_dirty;
  int move_count;
  int paused;
  int game_over;
  int reached_goal;
  Uint32 banner_until;
  int spawn_x;
  int spawn_y;
  Uint32 spawn_flash_until;
} game_state;

static const SDL_Color k_bg = {18, 22, 31, 255};
static const SDL_Color k_bg_grid = {27, 33, 47, 255};
static const SDL_Color k_frame = {74, 96, 126, 255};
static const SDL_Color k_board_shell = {35, 45, 61, 255};
static const SDL_Color k_board_bg = {46, 58, 77, 255};
static const SDL_Color k_panel = {26, 33, 45, 255};
static const SDL_Color k_panel_hi = {38, 51, 71, 255};
static const SDL_Color k_text = {236, 240, 245, 255};
static const SDL_Color k_muted = {157, 168, 188, 255};
static const SDL_Color k_accent = {114, 236, 220, 255};
static const SDL_Color k_overlay = {7, 11, 17, 224};
static const SDL_Color k_dark_text = {27, 31, 41, 255};

static void audio_callback(void* userdata, Uint8* stream, int length) {
  tone_state* state = (tone_state*)userdata;
  int16_t* samples = (int16_t*)stream;
  int count = length / (int)sizeof(int16_t);
  int index = 0;
  for (index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (state->frames_remaining > 0) {
      sample = (state->phase < 3.14159f) ? 1400 : -1400;
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

static int minimum(int left, int right) {
  return left < right ? left : right;
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
  static const uint8_t slash[7] = {1, 2, 2, 4, 8, 8, 16};

  const uint8_t* glyph = blank;
  switch (ch) {
    case 'A': glyph = a; break; case 'B': glyph = b; break; case 'C': glyph = c; break;
    case 'D': glyph = d; break; case 'E': glyph = e; break; case 'F': glyph = f; break;
    case 'G': glyph = g; break; case 'H': glyph = h; break; case 'I': glyph = i; break;
    case 'J': glyph = j; break; case 'K': glyph = k; break; case 'L': glyph = l; break;
    case 'M': glyph = m; break; case 'N': glyph = n; break; case 'O': glyph = o; break;
    case 'P': glyph = p; break; case 'Q': glyph = q; break; case 'R': glyph = r; break;
    case 'S': glyph = s; break; case 'T': glyph = t; break; case 'U': glyph = u; break;
    case 'V': glyph = v; break; case 'W': glyph = w; break; case 'X': glyph = x; break;
    case 'Y': glyph = y; break; case 'Z': glyph = z; break; case '0': glyph = n0; break;
    case '1': glyph = n1; break; case '2': glyph = n2; break; case '3': glyph = n3; break;
    case '4': glyph = n4; break; case '5': glyph = n5; break; case '6': glyph = n6; break;
    case '7': glyph = n7; break; case '8': glyph = n8; break; case '9': glyph = n9; break;
    case ':': glyph = colon; break; case '-': glyph = dash; break; case '/': glyph = slash; break;
    default: break;
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

static void draw_text_right(SDL_Renderer* renderer, const char* text, int right_x, int y, int scale, SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, 0);
}

static void build_path(char* output, size_t output_size, const char* dir, const char* file_name) {
  size_t dir_len = 0;
  if (output == NULL || output_size == 0) {
    return;
  }
  if (dir == NULL || dir[0] == '\0') {
    snprintf(output, output_size, "%s", file_name);
    return;
  }
  dir_len = strlen(dir);
  if (dir[dir_len - 1] == KILOBYTES_PATH_SEPARATOR) {
    snprintf(output, output_size, "%s%s", dir, file_name);
  } else {
    snprintf(output, output_size, "%s%c%s", dir, KILOBYTES_PATH_SEPARATOR, file_name);
  }
}

static void load_best_score(const pp_context* context, game_state* game) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  build_path(path, sizeof(path), pp_get_save_dir(context), "kilobytes-best.txt");
  file = fopen(path, "r");
  if (file == NULL) {
    game->best_score = 0;
    game->best_dirty = 0;
    return;
  }
  if (fscanf(file, "%d", &game->best_score) != 1) {
    game->best_score = 0;
  }
  game->best_dirty = 0;
  fclose(file);
}

static void save_best_score(const pp_context* context, const game_state* game) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  build_path(path, sizeof(path), pp_get_save_dir(context), "kilobytes-best.txt");
  file = fopen(path, "w");
  if (file == NULL) {
    return;
  }
  fprintf(file, "%d\n", game->best_score);
  fclose(file);
}

static void clear_board(game_state* game) {
  int row = 0;
  int column = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      game->board[row][column] = 0;
    }
  }
}

static int top_tile(const game_state* game) {
  int best = 0;
  int row = 0;
  int column = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      best = maximum(best, game->board[row][column]);
    }
  }
  return best;
}

static int has_empty_cell(const game_state* game) {
  int row = 0;
  int column = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      if (game->board[row][column] == 0) {
        return 1;
      }
    }
  }
  return 0;
}

static void spawn_tile(game_state* game, Uint32 now) {
  int empty_positions[GRID_SIZE * GRID_SIZE][2];
  int count = 0;
  int row = 0;
  int column = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      if (game->board[row][column] == 0) {
        empty_positions[count][0] = row;
        empty_positions[count][1] = column;
        ++count;
      }
    }
  }
  if (count == 0) {
    game->spawn_x = -1;
    game->spawn_y = -1;
    game->spawn_flash_until = 0;
    return;
  }

  {
    int pick = rand() % count;
    row = empty_positions[pick][0];
    column = empty_positions[pick][1];
  }
  game->board[row][column] = (rand() % 10 == 0) ? 4 : 2;
  game->spawn_y = row;
  game->spawn_x = column;
  game->spawn_flash_until = now + 160;
}

static int line_changed(const int before[GRID_SIZE], const int after[GRID_SIZE]) {
  int index = 0;
  for (index = 0; index < GRID_SIZE; ++index) {
    if (before[index] != after[index]) {
      return 1;
    }
  }
  return 0;
}

static int slide_line_left(int line[GRID_SIZE], int* gained_score) {
  int compact[GRID_SIZE] = {0, 0, 0, 0};
  int merged[GRID_SIZE] = {0, 0, 0, 0};
  int write_index = 0;
  int index = 0;
  int changed = 0;
  int before[GRID_SIZE];

  for (index = 0; index < GRID_SIZE; ++index) {
    before[index] = line[index];
    if (line[index] != 0) {
      compact[write_index] = line[index];
      ++write_index;
    }
  }

  write_index = 0;
  for (index = 0; index < GRID_SIZE; ++index) {
    if (compact[index] == 0) {
      continue;
    }
    if (index + 1 < GRID_SIZE && compact[index] == compact[index + 1]) {
      merged[write_index] = compact[index] * 2;
      *gained_score += merged[write_index];
      ++write_index;
      ++index;
    } else {
      merged[write_index] = compact[index];
      ++write_index;
    }
  }

  for (index = 0; index < GRID_SIZE; ++index) {
    line[index] = merged[index];
  }
  changed = line_changed(before, line);
  return changed;
}

static int move_left(game_state* game) {
  int row = 0;
  int changed = 0;
  int gained = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    if (slide_line_left(game->board[row], &gained)) {
      changed = 1;
    }
  }
  game->score += gained;
  return changed;
}

static int move_right(game_state* game) {
  int row = 0;
  int changed = 0;
  int gained = 0;
  for (row = 0; row < GRID_SIZE; ++row) {
    int line[GRID_SIZE];
    int index = 0;
    for (index = 0; index < GRID_SIZE; ++index) {
      line[index] = game->board[row][GRID_SIZE - 1 - index];
    }
    if (slide_line_left(line, &gained)) {
      changed = 1;
    }
    for (index = 0; index < GRID_SIZE; ++index) {
      game->board[row][GRID_SIZE - 1 - index] = line[index];
    }
  }
  game->score += gained;
  return changed;
}

static int move_up(game_state* game) {
  int column = 0;
  int changed = 0;
  int gained = 0;
  for (column = 0; column < GRID_SIZE; ++column) {
    int line[GRID_SIZE];
    int index = 0;
    for (index = 0; index < GRID_SIZE; ++index) {
      line[index] = game->board[index][column];
    }
    if (slide_line_left(line, &gained)) {
      changed = 1;
    }
    for (index = 0; index < GRID_SIZE; ++index) {
      game->board[index][column] = line[index];
    }
  }
  game->score += gained;
  return changed;
}

static int move_down(game_state* game) {
  int column = 0;
  int changed = 0;
  int gained = 0;
  for (column = 0; column < GRID_SIZE; ++column) {
    int line[GRID_SIZE];
    int index = 0;
    for (index = 0; index < GRID_SIZE; ++index) {
      line[index] = game->board[GRID_SIZE - 1 - index][column];
    }
    if (slide_line_left(line, &gained)) {
      changed = 1;
    }
    for (index = 0; index < GRID_SIZE; ++index) {
      game->board[GRID_SIZE - 1 - index][column] = line[index];
    }
  }
  game->score += gained;
  return changed;
}

static int can_make_move(const game_state* game) {
  int row = 0;
  int column = 0;
  if (has_empty_cell(game)) {
    return 1;
  }
  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      if (column + 1 < GRID_SIZE && game->board[row][column] == game->board[row][column + 1]) {
        return 1;
      }
      if (row + 1 < GRID_SIZE && game->board[row][column] == game->board[row + 1][column]) {
        return 1;
      }
    }
  }
  return 0;
}

static void reset_game(game_state* game, Uint32 now) {
  clear_board(game);
  game->score = 0;
  game->move_count = 0;
  game->paused = 0;
  game->game_over = 0;
  game->reached_goal = 0;
  game->banner_until = 0;
  game->spawn_x = -1;
  game->spawn_y = -1;
  game->spawn_flash_until = 0;
  spawn_tile(game, now);
  spawn_tile(game, now);
}

static void tile_label(int value, char* buffer, size_t buffer_size) {
  if (value < 128) {
    snprintf(buffer, buffer_size, "%dB", value);
  } else if (value < 1024) {
    snprintf(buffer, buffer_size, "%d", value);
  } else if (value % 1024 == 0) {
    snprintf(buffer, buffer_size, "%dKB", value / 1024);
  } else {
    snprintf(buffer, buffer_size, "%d", value);
  }
}

static SDL_Color tile_color(int value) {
  switch (value) {
    case 0: return (SDL_Color){64, 77, 98, 255};
    case 2: return (SDL_Color){226, 231, 238, 255};
    case 4: return (SDL_Color){205, 221, 255, 255};
    case 8: return (SDL_Color){156, 226, 246, 255};
    case 16: return (SDL_Color){109, 220, 221, 255};
    case 32: return (SDL_Color){106, 208, 166, 255};
    case 64: return (SDL_Color){153, 223, 125, 255};
    case 128: return (SDL_Color){236, 213, 98, 255};
    case 256: return (SDL_Color){246, 186, 88, 255};
    case 512: return (SDL_Color){240, 144, 84, 255};
    case 1024: return (SDL_Color){237, 108, 92, 255};
    case 2048: return (SDL_Color){228, 84, 115, 255};
    default: return (SDL_Color){182, 106, 216, 255};
  }
}

static SDL_Color tile_text_color(int value) {
  if (value == 0) {
    return k_muted;
  }
  if (value <= 8) {
    return k_dark_text;
  }
  return k_text;
}

static void draw_stat_box(SDL_Renderer* renderer, int x, int y, int w, const char* label, int value) {
  char number[32];
  fill_rect(renderer, (SDL_Rect){x, y, w, 52}, k_panel_hi);
  stroke_rect(renderer, (SDL_Rect){x, y, w, 52}, k_frame);
  draw_text(renderer, label, x + 10, y + 8, 1, k_muted, 0);
  snprintf(number, sizeof(number), "%d", value);
  draw_text_right(renderer, number, x + w - 10, y + 24, 2, k_text);
}

static void draw_board(SDL_Renderer* renderer, const game_state* game, Uint32 now) {
  int row = 0;
  int column = 0;
  fill_rect(renderer, (SDL_Rect){BOARD_X - 8, BOARD_Y - 8, BOARD_W + 16, BOARD_H + 16}, k_frame);
  fill_rect(renderer, (SDL_Rect){BOARD_X, BOARD_Y, BOARD_W, BOARD_H}, k_board_shell);
  fill_rect(renderer, (SDL_Rect){BOARD_X + 6, BOARD_Y + 6, BOARD_W - 12, BOARD_H - 12}, k_board_bg);

  for (row = 0; row < GRID_SIZE; ++row) {
    for (column = 0; column < GRID_SIZE; ++column) {
      int value = game->board[row][column];
      SDL_Color color = tile_color(value);
      SDL_Rect tile = {
          BOARD_X + TILE_GAP + column * (TILE_SIZE + TILE_GAP),
          BOARD_Y + TILE_GAP + row * (TILE_SIZE + TILE_GAP),
          TILE_SIZE,
          TILE_SIZE
      };
      char label[16];
      int scale = 2;
      fill_rect(renderer, tile, color);
      stroke_rect(renderer, tile, (SDL_Color){255, 255, 255, 28});
      if (game->spawn_x == column && game->spawn_y == row && now < game->spawn_flash_until) {
        stroke_rect(renderer, (SDL_Rect){tile.x - 1, tile.y - 1, tile.w + 2, tile.h + 2}, k_accent);
        stroke_rect(renderer, (SDL_Rect){tile.x - 2, tile.y - 2, tile.w + 4, tile.h + 4}, k_accent);
      }
      if (value == 0) {
        draw_text(renderer, "--", tile.x + tile.w / 2, tile.y + 22, 2, k_muted, 1);
        continue;
      }
      tile_label(value, label, sizeof(label));
      if ((int)strlen(label) >= 4) {
        scale = 1;
      } else if ((int)strlen(label) == 3) {
        scale = 2;
      } else {
        scale = 3;
      }
      draw_text(renderer, label, tile.x + tile.w / 2, tile.y + (value >= 1024 ? 24 : 20), scale, tile_text_color(value), 1);
    }
  }
}

static void draw_panel(SDL_Renderer* renderer, const game_state* game) {
  char label[24];
  fill_rect(renderer, (SDL_Rect){PANEL_X + 6, PANEL_Y + 8, PANEL_W, PANEL_H}, k_frame);
  fill_rect(renderer, (SDL_Rect){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, k_panel);
  stroke_rect(renderer, (SDL_Rect){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, k_frame);

  draw_stat_box(renderer, PANEL_X + 10, PANEL_Y + 12, PANEL_W - 20, "SCORE", game->score);
  draw_stat_box(renderer, PANEL_X + 10, PANEL_Y + 70, PANEL_W - 20, "BEST", game->best_score);

  fill_rect(renderer, (SDL_Rect){PANEL_X + 10, PANEL_Y + 128, PANEL_W - 20, 62}, k_panel_hi);
  stroke_rect(renderer, (SDL_Rect){PANEL_X + 10, PANEL_Y + 128, PANEL_W - 20, 62}, k_frame);
  draw_text(renderer, "TOP BLOCK", PANEL_X + 20, PANEL_Y + 138, 1, k_muted, 0);
  tile_label(top_tile(game), label, sizeof(label));
  draw_text(renderer, label, PANEL_X + PANEL_W / 2, PANEL_Y + 156, 3, k_text, 1);

  fill_rect(renderer, (SDL_Rect){PANEL_X + 10, PANEL_Y + 202, PANEL_W - 20, 84}, k_panel_hi);
  stroke_rect(renderer, (SDL_Rect){PANEL_X + 10, PANEL_Y + 202, PANEL_W - 20, 84}, k_frame);
  draw_text(renderer, "D PAD SHIFT", PANEL_X + 18, PANEL_Y + 214, 1, k_text, 0);
  draw_text(renderer, "B NEW BOARD", PANEL_X + 18, PANEL_Y + 232, 1, k_text, 0);
  draw_text(renderer, "START PAUSE", PANEL_X + 18, PANEL_Y + 250, 1, k_text, 0);
  draw_text(renderer, "SELECT EXIT", PANEL_X + 18, PANEL_Y + 268, 1, k_text, 0);

  draw_text(renderer, "MOVES", PANEL_X + 12, PANEL_Y + 302, 2, k_muted, 0);
  snprintf(label, sizeof(label), "%d", game->move_count);
  draw_text_right(renderer, label, PANEL_X + PANEL_W - 12, PANEL_Y + 302, 2, k_text);

  draw_text(renderer, "TARGET 2KB", PANEL_X + PANEL_W / 2, PANEL_Y + 340, 2, k_accent, 1);
  draw_text(renderer, "MERGE MEMORY", PANEL_X + PANEL_W / 2, PANEL_Y + 364, 1, k_muted, 1);
}

static void draw_overlay(SDL_Renderer* renderer, const char* title, const char* subtitle) {
  fill_rect(renderer, (SDL_Rect){72, 208, 368, 94}, k_overlay);
  stroke_rect(renderer, (SDL_Rect){72, 208, 368, 94}, k_frame);
  draw_text(renderer, title, VIRTUAL_WIDTH / 2, 224, 4, k_text, 1);
  draw_text(renderer, subtitle, VIRTUAL_WIDTH / 2, 260, 2, k_muted, 1);
}

static int perform_move(game_state* game, int direction, Uint32 now) {
  int changed = 0;
  if (direction == 0) {
    changed = move_up(game);
  } else if (direction == 1) {
    changed = move_right(game);
  } else if (direction == 2) {
    changed = move_down(game);
  } else if (direction == 3) {
    changed = move_left(game);
  }
  if (!changed) {
    if (!can_make_move(game)) {
      game->game_over = 1;
    }
    return 0;
  }

  ++game->move_count;
  if (game->score > game->best_score) {
    game->best_score = game->score;
    game->best_dirty = 1;
  }
  spawn_tile(game, now);
  if (!game->reached_goal && top_tile(game) >= GOAL_TILE) {
    game->reached_goal = 1;
    game->banner_until = now + 2200;
  }
  if (!can_make_move(game)) {
    game->game_over = 1;
  }
  return 1;
}

int main(int argc, char** argv) {
  pp_context context;
  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  SDL_AudioDeviceID audio_device = 0U;
  pp_audio_spec audio_spec;
  tone_state tone;
  game_state game;
  pp_input_state input;
  pp_input_state previous;
  int width = 0;
  int height = 0;
  Uint32 seed = (Uint32)time(NULL);

  (void)argc;
  (void)argv;

  memset(&context, 0, sizeof(context));
  memset(&audio_spec, 0, sizeof(audio_spec));
  memset(&tone, 0, sizeof(tone));
  memset(&game, 0, sizeof(game));
  memset(&input, 0, sizeof(input));
  memset(&previous, 0, sizeof(previous));

  srand(seed);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (pp_init(&context, "kilobytes") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(width, 512);
  height = maximum(height, 512);

  window = SDL_CreateWindow("KILOBYTES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

  load_best_score(&context, &game);
  reset_game(&game, SDL_GetTicks());

  while (!pp_should_exit(&context)) {
    Uint32 now = SDL_GetTicks();
    int pressed_up = 0;
    int pressed_down = 0;
    int pressed_left = 0;
    int pressed_right = 0;
    int pressed_b = 0;
    int pressed_start = 0;
    int move_happened = 0;

    pp_poll_input(&context, &input);
    pressed_up = input.up && !previous.up;
    pressed_down = input.down && !previous.down;
    pressed_left = input.left && !previous.left;
    pressed_right = input.right && !previous.right;
    pressed_b = input.b && !previous.b;
    pressed_start = input.start && !previous.start;

    if (pressed_start) {
      if (game.game_over) {
        if (game.best_dirty) {
          save_best_score(&context, &game);
          game.best_dirty = 0;
        }
        reset_game(&game, now);
        trigger_tone(&tone, 760.0f, 72);
      } else {
        game.paused = !game.paused;
        trigger_tone(&tone, game.paused ? 240.0f : 620.0f, 52);
      }
    }

    if (pressed_b) {
      if (game.best_dirty) {
        save_best_score(&context, &game);
        game.best_dirty = 0;
      }
      reset_game(&game, now);
      trigger_tone(&tone, 420.0f, 56);
    }

    if (!game.paused && !game.game_over) {
      if (pressed_up) {
        move_happened = perform_move(&game, 0, now);
      } else if (pressed_right) {
        move_happened = perform_move(&game, 1, now);
      } else if (pressed_down) {
        move_happened = perform_move(&game, 2, now);
      } else if (pressed_left) {
        move_happened = perform_move(&game, 3, now);
      }
      if (move_happened) {
        trigger_tone(&tone, 760.0f, 40);
      } else if (pressed_up || pressed_right || pressed_down || pressed_left) {
        trigger_tone(&tone, 300.0f, 28);
      }
    }

    fill_rect(renderer, (SDL_Rect){0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT}, k_bg);
    fill_rect(renderer, (SDL_Rect){0, 86, VIRTUAL_WIDTH, 2}, k_bg_grid);
    draw_text(renderer, "KILOBYTES", VIRTUAL_WIDTH / 2, 28, 5, k_text, 1);
    draw_text(renderer, "MERGE MEMORY BLOCKS TO REACH 2KB", VIRTUAL_WIDTH / 2, 72, 1, k_muted, 1);

    draw_board(renderer, &game, now);
    draw_panel(renderer, &game);

    if (game.paused && !game.game_over) {
      draw_overlay(renderer, "PAUSED", "START TO RESUME");
    } else if (game.game_over) {
      draw_overlay(renderer, "OUT OF SPACE", "B FOR NEW BOARD");
    } else if (game.reached_goal && now < game.banner_until) {
      draw_overlay(renderer, "2KB REACHED", "KEEP GOING OR HIT B");
    }

    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }

  if (game.best_dirty) {
    save_best_score(&context, &game);
  }

  if (audio_device != 0U) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
