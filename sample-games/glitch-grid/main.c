#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define GLITCHGRID_PATH_SEPARATOR '\\'
#else
#define GLITCHGRID_PATH_SEPARATOR '/'
#endif

#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512
#define GRID_WIDTH 12
#define GRID_HEIGHT 12
#define CELL_SIZE 24
#define BOARD_OFFSET_X 36
#define BOARD_OFFSET_Y 116
#define PANEL_X 332
#define PANEL_Y 116
#define PANEL_W 150
#define PANEL_H 288
#define BASE_GLITCH_COUNT 20
#define MAX_GLITCH_COUNT 38

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

typedef struct tile {
  int glitch;
  int revealed;
  int flagged;
  int adjacent;
} tile;

static const SDL_Color k_bg = {13, 24, 31, 255};
static const SDL_Color k_grid_shadow = {9, 14, 19, 255};
static const SDL_Color k_grid_frame = {37, 72, 82, 255};
static const SDL_Color k_grid_bg = {18, 37, 46, 255};
static const SDL_Color k_tile_hidden = {28, 58, 66, 255};
static const SDL_Color k_tile_hidden_bright = {42, 86, 95, 255};
static const SDL_Color k_tile_open = {163, 207, 182, 255};
static const SDL_Color k_tile_open_shadow = {124, 166, 147, 255};
static const SDL_Color k_tile_cursor = {120, 244, 235, 255};
static const SDL_Color k_panel = {20, 41, 50, 255};
static const SDL_Color k_panel_highlight = {31, 70, 82, 255};
static const SDL_Color k_text = {193, 247, 226, 255};
static const SDL_Color k_muted = {111, 177, 160, 255};
static const SDL_Color k_corrupt = {240, 84, 94, 255};
static const SDL_Color k_marker = {255, 210, 110, 255};
static const SDL_Color k_overlay = {8, 15, 21, 216};

static void audio_callback(void* userdata, Uint8* stream, int length) {
  tone_state* state = (tone_state*)userdata;
  int16_t* samples = (int16_t*)stream;
  int count = length / (int)sizeof(int16_t);
  int index = 0;
  for (index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (state->frames_remaining > 0) {
      sample = (state->phase < 3.14159f) ? 1500 : -1500;
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

static int clamp_int(int value, int low, int high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
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

static void draw_text_right(SDL_Renderer* renderer,
                            const char* text,
                            int right_x,
                            int y,
                            int scale,
                            SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, 0);
}

static void save_best_time(const pp_context* context, int best_time_ms) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  snprintf(path, sizeof(path), "%s%cbest_time.txt", pp_get_save_dir(context), GLITCHGRID_PATH_SEPARATOR);
  file = fopen(path, "w");
  if (file == NULL) {
    return;
  }
  fprintf(file, "%d\n", best_time_ms);
  fclose(file);
}

static int load_best_time(const pp_context* context) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  int value = 0;
  snprintf(path, sizeof(path), "%s%cbest_time.txt", pp_get_save_dir(context), GLITCHGRID_PATH_SEPARATOR);
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

static void clear_board(tile board[GRID_HEIGHT][GRID_WIDTH]) {
  int y = 0;
  for (y = 0; y < GRID_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < GRID_WIDTH; ++x) {
      board[y][x].glitch = 0;
      board[y][x].revealed = 0;
      board[y][x].flagged = 0;
      board[y][x].adjacent = 0;
    }
  }
}

static int in_bounds(int x, int y) {
  return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

static void compute_adjacency(tile board[GRID_HEIGHT][GRID_WIDTH]) {
  int y = 0;
  for (y = 0; y < GRID_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < GRID_WIDTH; ++x) {
      int count = 0;
      int dy = 0;
      int dx = 0;
      if (board[y][x].glitch) {
        board[y][x].adjacent = -1;
        continue;
      }
      for (dy = -1; dy <= 1; ++dy) {
        for (dx = -1; dx <= 1; ++dx) {
          if ((dx != 0 || dy != 0) && in_bounds(x + dx, y + dy) && board[y + dy][x + dx].glitch) {
            ++count;
          }
        }
      }
      board[y][x].adjacent = count;
    }
  }
}

static int glitch_count_for_level(int level) {
  int count = BASE_GLITCH_COUNT + (level - 1) * 2;
  return minimum(MAX_GLITCH_COUNT, maximum(BASE_GLITCH_COUNT, count));
}

static void place_glitches(tile board[GRID_HEIGHT][GRID_WIDTH], int safe_x, int safe_y, int glitch_count) {
  int placed = 0;
  while (placed < glitch_count) {
    int x = rand() % GRID_WIDTH;
    int y = rand() % GRID_HEIGHT;
    if (board[y][x].glitch) {
      continue;
    }
    if (x >= safe_x - 1 && x <= safe_x + 1 && y >= safe_y - 1 && y <= safe_y + 1) {
      continue;
    }
    board[y][x].glitch = 1;
    ++placed;
  }
}

static void flood_reveal(tile board[GRID_HEIGHT][GRID_WIDTH], int start_x, int start_y, int* revealed_safe) {
  int stack_x[GRID_WIDTH * GRID_HEIGHT];
  int stack_y[GRID_WIDTH * GRID_HEIGHT];
  int top = 0;
  stack_x[top] = start_x;
  stack_y[top] = start_y;
  ++top;

  while (top > 0) {
    int x = stack_x[top - 1];
    int y = stack_y[top - 1];
    int dx = 0;
    int dy = 0;
    --top;

    if (!in_bounds(x, y) || board[y][x].revealed || board[y][x].flagged || board[y][x].glitch) {
      continue;
    }

    board[y][x].revealed = 1;
    ++(*revealed_safe);
    if (board[y][x].adjacent != 0) {
      continue;
    }

    for (dy = -1; dy <= 1; ++dy) {
      for (dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        if (in_bounds(x + dx, y + dy) && !board[y + dy][x + dx].revealed && !board[y + dy][x + dx].glitch) {
          stack_x[top] = x + dx;
          stack_y[top] = y + dy;
          ++top;
        }
      }
    }
  }
}

static void begin_grid(tile board[GRID_HEIGHT][GRID_WIDTH], int first_x, int first_y, int glitch_count) {
  clear_board(board);
  place_glitches(board, first_x, first_y, glitch_count);
  compute_adjacency(board);
}

static void reveal_tile(tile board[GRID_HEIGHT][GRID_WIDTH],
                        int x,
                        int y,
                        int* revealed_safe,
                        int* triggered_glitch) {
  if (!in_bounds(x, y) || board[y][x].revealed || board[y][x].flagged) {
    return;
  }
  if (board[y][x].glitch) {
    board[y][x].revealed = 1;
    *triggered_glitch = 1;
    return;
  }
  flood_reveal(board, x, y, revealed_safe);
}

static int count_flags(tile board[GRID_HEIGHT][GRID_WIDTH]) {
  int count = 0;
  int y = 0;
  for (y = 0; y < GRID_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < GRID_WIDTH; ++x) {
      if (board[y][x].flagged) {
        ++count;
      }
    }
  }
  return count;
}

static void draw_glitch(SDL_Renderer* renderer, int center_x, int center_y) {
  fill_circle(renderer, center_x, center_y, 7, k_corrupt);
  fill_rect(renderer, (SDL_Rect){center_x - 1, center_y - 9, 2, 18}, k_bg);
  fill_rect(renderer, (SDL_Rect){center_x - 9, center_y - 1, 18, 2}, k_bg);
  fill_circle(renderer, center_x, center_y, 3, k_marker);
}

static SDL_Color number_color(int value) {
  switch (value) {
    case 1: return (SDL_Color){91, 171, 255, 255};
    case 2: return (SDL_Color){106, 219, 145, 255};
    case 3: return (SDL_Color){255, 137, 121, 255};
    case 4: return (SDL_Color){178, 139, 255, 255};
    case 5: return (SDL_Color){255, 197, 98, 255};
    case 6: return (SDL_Color){110, 238, 229, 255};
    case 7: return (SDL_Color){255, 214, 219, 255};
    case 8: return (SDL_Color){214, 247, 99, 255};
    default: return k_text;
  }
}

static void draw_board(SDL_Renderer* renderer,
                       tile board[GRID_HEIGHT][GRID_WIDTH],
                       int cursor_x,
                       int cursor_y,
                       int show_all,
                       Uint32 ticks) {
  int y = 0;
  fill_rect(renderer, (SDL_Rect){BOARD_OFFSET_X - 10, BOARD_OFFSET_Y - 10,
                                 GRID_WIDTH * CELL_SIZE + 20, GRID_HEIGHT * CELL_SIZE + 20},
            k_grid_shadow);
  fill_rect(renderer, (SDL_Rect){BOARD_OFFSET_X - 14, BOARD_OFFSET_Y - 14,
                                 GRID_WIDTH * CELL_SIZE + 20, GRID_HEIGHT * CELL_SIZE + 20},
            k_grid_frame);
  fill_rect(renderer, (SDL_Rect){BOARD_OFFSET_X - 8, BOARD_OFFSET_Y - 8,
                                 GRID_WIDTH * CELL_SIZE + 8, GRID_HEIGHT * CELL_SIZE + 8},
            k_grid_bg);

  for (y = 0; y < GRID_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < GRID_WIDTH; ++x) {
      const int px = BOARD_OFFSET_X + x * CELL_SIZE;
      const int py = BOARD_OFFSET_Y + y * CELL_SIZE;
      const int visible = board[y][x].revealed || (show_all && board[y][x].glitch);
      SDL_Rect tile_rect = {px, py, CELL_SIZE - 2, CELL_SIZE - 2};

      fill_rect(renderer, tile_rect, visible ? k_tile_open : k_tile_hidden);
      if (!visible) {
        fill_rect(renderer, (SDL_Rect){px + 2, py + 2, CELL_SIZE - 8, CELL_SIZE - 8}, k_tile_hidden_bright);
      } else {
        fill_rect(renderer, (SDL_Rect){px + 2, py + 2, CELL_SIZE - 8, CELL_SIZE - 8}, k_tile_open_shadow);
      }

      if (visible && board[y][x].glitch) {
        draw_glitch(renderer, px + (CELL_SIZE / 2) - 1, py + (CELL_SIZE / 2) - 1);
      } else if (visible && board[y][x].adjacent > 0) {
        char text[2];
        text[0] = (char)('0' + board[y][x].adjacent);
        text[1] = '\0';
        draw_text(renderer, text, px + CELL_SIZE / 2 - 1, py + 7, 2, number_color(board[y][x].adjacent), 1);
      } else if (!visible && board[y][x].flagged) {
        fill_rect(renderer, (SDL_Rect){px + 8, py + 6, 2, 12}, k_marker);
        fill_rect(renderer, (SDL_Rect){px + 10, py + 6, 8, 6}, k_marker);
        fill_rect(renderer, (SDL_Rect){px + 7, py + 17, 8, 2}, k_text);
      }

      if (x == cursor_x && y == cursor_y) {
        SDL_Color cursor = ((ticks / 160U) % 2U == 0U) ? k_tile_cursor : k_text;
        stroke_rect(renderer, (SDL_Rect){px - 2, py - 2, CELL_SIZE + 2, CELL_SIZE + 2}, cursor);
      }
    }
  }
}

static void draw_sidebar(SDL_Renderer* renderer,
                         int level,
                         int glitch_count,
                         int elapsed_ms,
                         int best_time_ms,
                         int revealed_safe,
                         int flags_used) {
  char buffer[64];
  const int left_x = PANEL_X + 16;
  const int right_x = PANEL_X + PANEL_W - 16;
  fill_rect(renderer, (SDL_Rect){PANEL_X + 6, PANEL_Y + 8, PANEL_W, PANEL_H}, k_grid_shadow);
  fill_rect(renderer, (SDL_Rect){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, k_panel);
  stroke_rect(renderer, (SDL_Rect){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, k_grid_frame);

  draw_text(renderer, "TIME", left_x, PANEL_Y + 18, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%02d:%02d", elapsed_ms / 60000, (elapsed_ms / 1000) % 60);
  draw_text_right(renderer, buffer, right_x, PANEL_Y + 32, 2, k_text);

  draw_text(renderer, "BEST", left_x, PANEL_Y + 70, 1, k_muted, 0);
  if (best_time_ms > 0) {
    snprintf(buffer, sizeof(buffer), "%02d:%02d", best_time_ms / 60000, (best_time_ms / 1000) % 60);
  } else {
    snprintf(buffer, sizeof(buffer), "--:--");
  }
  draw_text_right(renderer, buffer, right_x, PANEL_Y + 84, 2, k_text);

  draw_text(renderer, "LEVEL", left_x, PANEL_Y + 122, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%d", level);
  draw_text_right(renderer, buffer, right_x, PANEL_Y + 136, 2, k_text);

  draw_text(renderer, "SAFE", left_x, PANEL_Y + 164, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%d/%d", revealed_safe, GRID_WIDTH * GRID_HEIGHT - glitch_count);
  draw_text_right(renderer, buffer, right_x, PANEL_Y + 178, 2, k_text);

  draw_text(renderer, "FLAGS", left_x, PANEL_Y + 206, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%d/%d", flags_used, glitch_count);
  draw_text_right(renderer, buffer, right_x, PANEL_Y + 220, 2, k_text);

  fill_rect(renderer, (SDL_Rect){PANEL_X + 12, PANEL_Y + 250, PANEL_W - 24, 70}, k_panel_highlight);
  stroke_rect(renderer, (SDL_Rect){PANEL_X + 12, PANEL_Y + 250, PANEL_W - 24, 70}, k_grid_frame);
  draw_text(renderer, "SCAN", PANEL_X + 20, PANEL_Y + 262, 1, k_muted, 0);
  draw_text_right(renderer, "A", PANEL_X + PANEL_W - 20, PANEL_Y + 262, 1, k_text);
  draw_text(renderer, "MARK", PANEL_X + 20, PANEL_Y + 280, 1, k_muted, 0);
  draw_text_right(renderer, "B", PANEL_X + PANEL_W - 20, PANEL_Y + 280, 1, k_text);
  draw_text(renderer, "MOVE", PANEL_X + 20, PANEL_Y + 298, 1, k_muted, 0);
  draw_text_right(renderer, "DPAD", PANEL_X + PANEL_W - 20, PANEL_Y + 298, 1, k_text);
}

static void draw_overlay(SDL_Renderer* renderer, const char* title, const char* subtitle, const char* prompt) {
  fill_rect(renderer, (SDL_Rect){86, 188, 340, 104}, k_overlay);
  stroke_rect(renderer, (SDL_Rect){86, 188, 340, 104}, k_grid_frame);
  draw_text(renderer, title, VIRTUAL_WIDTH / 2, 206, 4, k_text, 1);
  draw_text(renderer, subtitle, VIRTUAL_WIDTH / 2, 240, 2, k_muted, 1);
  draw_text(renderer, prompt, VIRTUAL_WIDTH / 2, 266, 1, k_text, 1);
}

static void render_scene(SDL_Renderer* renderer,
                         tile board[GRID_HEIGHT][GRID_WIDTH],
                         int level,
                         int glitch_count,
                         int cursor_x,
                         int cursor_y,
                         int started,
                         int ready_prompt_visible,
                         int paused,
                         int game_over,
                         int won,
                         int elapsed_ms,
                         int best_time_ms,
                         int revealed_safe,
                         Uint32 ticks) {
  int flags_used = count_flags(board);
  fill_rect(renderer, (SDL_Rect){0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT}, k_bg);
  fill_rect(renderer, (SDL_Rect){0, 74, VIRTUAL_WIDTH, 6}, k_panel_highlight);

  draw_text(renderer, "GLITCH GRID", VIRTUAL_WIDTH / 2, 38, 5, k_text, 1);
  draw_text(renderer, "SCAN SAFE SECTORS / AVOID CORRUPTED NODES", VIRTUAL_WIDTH / 2, 84, 1, k_muted, 1);

  draw_board(renderer, board, cursor_x, cursor_y, game_over || won, ticks);
  draw_sidebar(renderer, level, glitch_count, elapsed_ms, best_time_ms, revealed_safe, flags_used);

  if (ready_prompt_visible && !started && !game_over && !won) {
    draw_overlay(renderer, "READY", "FIRST SCAN IS SAFE", "LEVEL CLIMBS AFTER EACH CLEAR");
  } else if (paused) {
    draw_overlay(renderer, "PAUSED", "GRID SCAN HALTED", "START TO RESUME");
  } else if (won) {
    draw_overlay(renderer, "SYSTEM STABLE", "ALL SAFE SECTORS CLEARED", "A OR START FOR NEXT LEVEL");
  } else if (game_over) {
    draw_overlay(renderer, "GRID CORRUPTED", "YOU HIT A GLITCH NODE", "A OR START FOR NEW GRID");
  }

  draw_text(renderer, "B DEBUG MARKER", 70, 466, 1, k_muted, 0);
  draw_text_right(renderer, "START SELECT EXIT", 446, 466, 1, k_muted);
}

static SDL_Renderer* create_renderer_with_fallback(SDL_Window* window) {
  SDL_Renderer* renderer = NULL;

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer != NULL) {
    return renderer;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer != NULL) {
    return renderer;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  return renderer;
}

int main(int argc, char** argv) {
  pp_context context;
  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  pp_audio_spec audio_spec;
  SDL_AudioDeviceID audio_device = 0U;
  pp_input_state input;
  pp_input_state previous_input;
  tone_state tone;
  tile board[GRID_HEIGHT][GRID_WIDTH];
  int cursor_x = 0;
  int cursor_y = 0;
  int started = 0;
  int ready_prompt_visible = 1;
  int paused = 0;
  int game_over = 0;
  int won = 0;
  int level = 1;
  int glitch_count = BASE_GLITCH_COUNT;
  int revealed_safe = 0;
  int best_time_ms = 0;
  int elapsed_snapshot = 0;
  Uint32 started_at = 0U;
  Uint32 paused_at = 0U;
  int width = 0;
  int height = 0;

  (void)argc;
  (void)argv;

  memset(&previous_input, 0, sizeof(previous_input));
  memset(&tone, 0, sizeof(tone));
  memset(&input, 0, sizeof(input));
  srand((unsigned int)time(NULL));
  clear_board(board);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (pp_init(&context, "glitch-grid") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(512, width);
  height = maximum(512, height);
  best_time_ms = load_best_time(&context);

  window = SDL_CreateWindow("Glitch Grid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                            height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  renderer = create_renderer_with_fallback(window);
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

  while (!pp_should_exit(&context)) {
    const Uint32 now = SDL_GetTicks();
    if (started && !paused && !game_over && !won) {
      elapsed_snapshot = (int)(now - started_at);
    }

    pp_poll_input(&context, &input);

    if ((input.left && !previous_input.left) && cursor_x > 0 && !paused && !game_over && !won) {
      cursor_x -= 1;
      ready_prompt_visible = 0;
      trigger_tone(&tone, 420.0f, 18);
    }
    if ((input.right && !previous_input.right) && cursor_x < GRID_WIDTH - 1 && !paused && !game_over && !won) {
      cursor_x += 1;
      ready_prompt_visible = 0;
      trigger_tone(&tone, 420.0f, 18);
    }
    if ((input.up && !previous_input.up) && cursor_y > 0 && !paused && !game_over && !won) {
      cursor_y -= 1;
      ready_prompt_visible = 0;
      trigger_tone(&tone, 420.0f, 18);
    }
    if ((input.down && !previous_input.down) && cursor_y < GRID_HEIGHT - 1 && !paused && !game_over && !won) {
      cursor_y += 1;
      ready_prompt_visible = 0;
      trigger_tone(&tone, 420.0f, 18);
    }

    if (input.start && !previous_input.start) {
      if (game_over || won) {
        const int advance_level = won;
        clear_board(board);
        started = 0;
        paused = 0;
        game_over = 0;
        won = 0;
        level = advance_level ? level + 1 : 1;
        glitch_count = glitch_count_for_level(level);
        revealed_safe = 0;
        elapsed_snapshot = 0;
        cursor_x = 0;
        cursor_y = 0;
        ready_prompt_visible = 1;
        started_at = 0U;
        paused_at = 0U;
        trigger_tone(&tone, 780.0f, 80);
      } else {
        if (!paused) {
          paused = 1;
          paused_at = now;
        } else {
          paused = 0;
          if (started && paused_at > 0U) {
            started_at += (now - paused_at);
          }
          paused_at = 0U;
        }
        trigger_tone(&tone, paused ? 280.0f : 620.0f, 48);
      }
    }

    if (input.b && !previous_input.b && !paused && !game_over && !won) {
      if (!board[cursor_y][cursor_x].revealed) {
        board[cursor_y][cursor_x].flagged = !board[cursor_y][cursor_x].flagged;
        trigger_tone(&tone, board[cursor_y][cursor_x].flagged ? 540.0f : 360.0f, 42);
      }
    }

    if ((input.a && !previous_input.a) && !paused) {
      if (game_over || won) {
        const int advance_level = won;
        clear_board(board);
        started = 0;
        paused = 0;
        game_over = 0;
        won = 0;
        level = advance_level ? level + 1 : 1;
        glitch_count = glitch_count_for_level(level);
        revealed_safe = 0;
        elapsed_snapshot = 0;
        cursor_x = 0;
        cursor_y = 0;
        ready_prompt_visible = 1;
        started_at = 0U;
        paused_at = 0U;
        trigger_tone(&tone, 780.0f, 80);
      } else {
        int triggered_glitch = 0;
        if (!started) {
          begin_grid(board, cursor_x, cursor_y, glitch_count);
          started = 1;
          started_at = now;
          elapsed_snapshot = 0;
        }
        reveal_tile(board, cursor_x, cursor_y, &revealed_safe, &triggered_glitch);
        if (triggered_glitch) {
          game_over = 1;
          elapsed_snapshot = (int)(now - started_at);
          trigger_tone(&tone, 160.0f, 240);
        } else if (revealed_safe >= GRID_WIDTH * GRID_HEIGHT - glitch_count) {
          won = 1;
          elapsed_snapshot = (int)(now - started_at);
          if (best_time_ms == 0 || elapsed_snapshot < best_time_ms) {
            best_time_ms = elapsed_snapshot;
            save_best_time(&context, best_time_ms);
          }
          trigger_tone(&tone, 920.0f, 180);
        } else {
          trigger_tone(&tone, 720.0f, 28);
        }
      }
    }

    render_scene(renderer, board, level, glitch_count, cursor_x, cursor_y, started, ready_prompt_visible,
                 paused, game_over, won,
                 elapsed_snapshot, best_time_ms, revealed_safe, now);
    SDL_RenderPresent(renderer);
    previous_input = input;
    SDL_Delay(16);
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
