#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define STONEFALL_PATH_SEPARATOR '\\'
#else
#define STONEFALL_PATH_SEPARATOR '/'
#endif

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define CELL_SIZE 18
#define BOARD_OFFSET_X 36
#define BOARD_OFFSET_Y 74
#define PANEL_OFFSET_X 248
#define PANEL_OFFSET_Y 74
#define PANEL_WIDTH 228
#define PANEL_HEIGHT 360
#define PREVIEW_OFFSET_X 302
#define PREVIEW_OFFSET_Y 124
#define PREVIEW_CELL_SIZE 16
#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

typedef struct stonefall_piece {
  int type;
  int rotation;
  int x;
  int y;
} stonefall_piece;

static const uint8_t k_piece_shapes[7][4][4][4] = {
    {{{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}, {0, 0, 1, 0}},
     {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}}},
    {{{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {{{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    {{{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}}},
    {{{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    {{{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}}},
    {{{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
     {{1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
};

static const SDL_Color k_piece_colors[8] = {
    {23, 33, 19, 255},   {94, 139, 82, 255},   {130, 176, 90, 255}, {76, 121, 70, 255},
    {167, 193, 105, 255}, {105, 156, 87, 255}, {59, 92, 51, 255},   {182, 209, 127, 255},
};

static const SDL_Color k_bg = {218, 223, 184, 255};
static const SDL_Color k_panel = {194, 201, 159, 255};
static const SDL_Color k_shadow = {150, 160, 118, 255};
static const SDL_Color k_dark = {26, 56, 28, 255};
static const SDL_Color k_muted = {67, 97, 55, 255};
static const SDL_Color k_border = {94, 123, 70, 255};
static const SDL_Color k_overlay = {24, 44, 22, 232};

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

static int random_piece_type(void) {
  return rand() % 7;
}

static void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

static int piece_fits(const int board[BOARD_HEIGHT][BOARD_WIDTH], stonefall_piece piece) {
  int py = 0;
  for (py = 0; py < 4; ++py) {
    int px = 0;
    for (px = 0; px < 4; ++px) {
      if (k_piece_shapes[piece.type][piece.rotation][py][px] == 0) {
        continue;
      }
      {
        const int board_x = piece.x + px;
        const int board_y = piece.y + py;
        if (board_x < 0 || board_x >= BOARD_WIDTH || board_y >= BOARD_HEIGHT) {
          return 0;
        }
        if (board_y >= 0 && board[board_y][board_x] != 0) {
          return 0;
        }
      }
    }
  }
  return 1;
}

static stonefall_piece make_piece(int type) {
  stonefall_piece piece;
  piece.type = type;
  piece.rotation = 0;
  piece.x = 3;
  piece.y = 0;
  return piece;
}

static void save_high_score(const pp_context* context, int high_score) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), STONEFALL_PATH_SEPARATOR);
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
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), STONEFALL_PATH_SEPARATOR);
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

static void lock_piece(int board[BOARD_HEIGHT][BOARD_WIDTH], stonefall_piece piece) {
  int py = 0;
  for (py = 0; py < 4; ++py) {
    int px = 0;
    for (px = 0; px < 4; ++px) {
      if (k_piece_shapes[piece.type][piece.rotation][py][px] == 0) {
        continue;
      }
      {
        const int board_x = piece.x + px;
        const int board_y = piece.y + py;
        if (board_y >= 0 && board_y < BOARD_HEIGHT && board_x >= 0 && board_x < BOARD_WIDTH) {
          board[board_y][board_x] = piece.type + 1;
        }
      }
    }
  }
}

static int clear_lines(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
  int cleared = 0;
  int y = BOARD_HEIGHT - 1;

  while (y >= 0) {
    int x = 0;
    int full = 1;
    for (x = 0; x < BOARD_WIDTH; ++x) {
      if (board[y][x] == 0) {
        full = 0;
        break;
      }
    }
    if (!full) {
      --y;
      continue;
    }

    ++cleared;
    for (; y > 0; --y) {
      for (x = 0; x < BOARD_WIDTH; ++x) {
        board[y][x] = board[y - 1][x];
      }
    }
    for (x = 0; x < BOARD_WIDTH; ++x) {
      board[0][x] = 0;
    }
    y = BOARD_HEIGHT - 1;
  }

  return cleared;
}

static int score_for_lines(int lines) {
  switch (lines) {
    case 1:
      return 100;
    case 2:
      return 300;
    case 3:
      return 500;
    case 4:
      return 800;
    default:
      return 0;
  }
}

static int maximum(int left, int right) {
  return left > right ? left : right;
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
  static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};
  static const uint8_t colon[7] = {0, 12, 12, 0, 12, 12, 0};
  static const uint8_t period[7] = {0, 0, 0, 0, 0, 12, 12};
  static const uint8_t slash[7] = {1, 2, 2, 4, 8, 8, 16};
  static const uint8_t percent[7] = {25, 25, 2, 4, 8, 19, 19};

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
    case '-': glyph = dash; break;
    case ':': glyph = colon; break;
    case '.': glyph = period; break;
    case '/': glyph = slash; break;
    case '%': glyph = percent; break;
    default: break;
  }
  return glyph[row];
}

static int text_width(const char* text, int scale) {
  size_t length = 0;
  if (text == NULL || text[0] == '\0') {
    return 0;
  }
  length = strlen(text);
  return (int)length * (6 * scale) - scale;
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

static void draw_panel(SDL_Renderer* renderer, SDL_Rect rect) {
  SDL_Rect shadow = {rect.x + 6, rect.y + 6, rect.w, rect.h};
  fill_rect(renderer, shadow, k_shadow);
  fill_rect(renderer, rect, k_panel);
  stroke_rect(renderer, rect, k_border);
}

static void draw_cell(SDL_Renderer* renderer, int x, int y, int size, SDL_Color color) {
  SDL_Rect outer = {x, y, size - 1, size - 1};
  SDL_Rect inner = {x + 3, y + 3, maximum(1, size - 7), maximum(1, size - 7)};
  fill_rect(renderer, outer, color);
  fill_rect(renderer, inner, k_dark);
}

static void draw_board_frame(SDL_Renderer* renderer) {
  SDL_Rect shadow = {BOARD_OFFSET_X - 8, BOARD_OFFSET_Y - 8, BOARD_WIDTH * CELL_SIZE + 16,
                     BOARD_HEIGHT * CELL_SIZE + 16};
  SDL_Rect frame = {BOARD_OFFSET_X - 12, BOARD_OFFSET_Y - 12, BOARD_WIDTH * CELL_SIZE + 24,
                    BOARD_HEIGHT * CELL_SIZE + 24};
  fill_rect(renderer, shadow, k_shadow);
  fill_rect(renderer, frame, k_panel);
  stroke_rect(renderer, frame, k_border);
}

static void draw_grid(SDL_Renderer* renderer, const int board[BOARD_HEIGHT][BOARD_WIDTH]) {
  int x = 0;
  int y = 0;
  for (y = 0; y < BOARD_HEIGHT; ++y) {
    for (x = 0; x < BOARD_WIDTH; ++x) {
      SDL_Rect cell = {BOARD_OFFSET_X + x * CELL_SIZE, BOARD_OFFSET_Y + y * CELL_SIZE,
                       CELL_SIZE - 1, CELL_SIZE - 1};
      fill_rect(renderer, cell, (SDL_Color){176, 189, 138, 255});
      stroke_rect(renderer, cell, (SDL_Color){159, 173, 122, 255});
      if (board[y][x] != 0) {
        draw_cell(renderer, cell.x, cell.y, CELL_SIZE, k_piece_colors[board[y][x]]);
      }
    }
  }
}

static void draw_piece_on_board(SDL_Renderer* renderer, stonefall_piece piece) {
  int x = 0;
  int y = 0;
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      if (k_piece_shapes[piece.type][piece.rotation][y][x] == 0) {
        continue;
      }
      if (piece.y + y < 0) {
        continue;
      }
      draw_cell(renderer, BOARD_OFFSET_X + (piece.x + x) * CELL_SIZE,
                BOARD_OFFSET_Y + (piece.y + y) * CELL_SIZE, CELL_SIZE,
                k_piece_colors[piece.type + 1]);
    }
  }
}

static void draw_preview_piece(SDL_Renderer* renderer, int next_type) {
  int x = 0;
  int y = 0;
  SDL_Rect box = {PREVIEW_OFFSET_X - 18, PREVIEW_OFFSET_Y - 18, 96, 96};
  fill_rect(renderer, box, (SDL_Color){205, 211, 170, 255});
  stroke_rect(renderer, box, k_border);

  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      if (k_piece_shapes[next_type][0][y][x] == 0) {
        continue;
      }
      draw_cell(renderer, PREVIEW_OFFSET_X + x * PREVIEW_CELL_SIZE,
                PREVIEW_OFFSET_Y + y * PREVIEW_CELL_SIZE, PREVIEW_CELL_SIZE,
                k_piece_colors[next_type + 1]);
    }
  }
}

static void draw_stat_line(SDL_Renderer* renderer, int y, const char* label, int value) {
  char buffer[64];
  draw_text(renderer, label, PANEL_OFFSET_X + 18, y, 1, k_muted, 0);
  snprintf(buffer, sizeof(buffer), "%d", value);
  draw_text(renderer, buffer, PANEL_OFFSET_X + PANEL_WIDTH - 18, y, 1, k_dark, 1);
}

static void draw_controls(SDL_Renderer* renderer) {
  int x = PANEL_OFFSET_X + 18;
  int y = 292;
  draw_text(renderer, "CONTROLS", x, y, 2, k_dark, 0);
  draw_text(renderer, "A OR UP ROTATE", x, y + 22, 2, k_muted, 0);
  draw_text(renderer, "D PAD MOVE", x, y + 42, 2, k_muted, 0);
  draw_text(renderer, "B HARD DROP", x, y + 62, 2, k_muted, 0);
  draw_text(renderer, "START PAUSE", x, y + 82, 2, k_muted, 0);
  draw_text(renderer, "START SELECT", x, y + 102, 2, k_muted, 0);
  draw_text(renderer, "HOLD TO EXIT", x, y + 122, 2, k_muted, 0);
}

static const char* current_tooltip(int paused, int game_over, int lines_cleared_last) {
  if (game_over) {
    return "TIP: PRESS A TO PLAY AGAIN";
  }
  if (paused) {
    return "TIP: PRESS START TO RESUME";
  }
  if (lines_cleared_last >= 4) {
    return "TIP: STONEFALL SCORES BIG";
  }
  return "TIP: B DROPS FAST, A ROTATES";
}

static void draw_overlay(SDL_Renderer* renderer,
                         const char* title,
                         const char* subtitle,
                         const char* action) {
  SDL_Rect overlay = {96, 154, 320, 144};
  fill_rect(renderer, overlay, k_overlay);
  stroke_rect(renderer, overlay, (SDL_Color){182, 209, 127, 255});
  draw_text(renderer, title, 256, 178, 4, k_bg, 1);
  draw_text(renderer, subtitle, 256, 220, 2, k_bg, 1);
  draw_text(renderer, action, 256, 252, 2, k_bg, 1);
}

static void draw_board(SDL_Renderer* renderer,
                       const int board[BOARD_HEIGHT][BOARD_WIDTH],
                       stonefall_piece active,
                       int paused,
                       int game_over,
                       int score,
                       int high_score,
                       int next_type,
                       int level,
                       int total_lines,
                       int lines_cleared_last) {
  SDL_Rect outer = {0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT};
  SDL_Rect header = {24, 24, 464, 34};
  SDL_Rect footer = {24, 466, 464, 22};
  SDL_Rect panel_rect = {PANEL_OFFSET_X, PANEL_OFFSET_Y, PANEL_WIDTH, PANEL_HEIGHT};

  fill_rect(renderer, outer, k_bg);
  fill_rect(renderer, header, k_panel);
  stroke_rect(renderer, header, k_border);
  draw_text(renderer, "STONEFALL", 256, 32, 3, k_dark, 1);

  draw_board_frame(renderer);
  draw_grid(renderer, board);
  draw_piece_on_board(renderer, active);
  draw_panel(renderer, panel_rect);

  draw_text(renderer, "NEXT", PANEL_OFFSET_X + 24, 84, 2, k_dark, 0);
  draw_preview_piece(renderer, next_type);
  draw_stat_line(renderer, 214, "SCORE", score);
  draw_stat_line(renderer, 232, "BEST", high_score);
  draw_stat_line(renderer, 250, "LEVEL", level);
  draw_stat_line(renderer, 268, "LINES", total_lines);
  draw_controls(renderer);

  fill_rect(renderer, footer, k_panel);
  stroke_rect(renderer, footer, k_border);
  draw_text(renderer, current_tooltip(paused, game_over, lines_cleared_last), 256, 472, 2,
            k_dark, 1);

  if (paused) {
    draw_overlay(renderer, "PAUSED", "TAKE FIVE", "PRESS START TO RESUME");
  }
  if (game_over) {
    draw_overlay(renderer, "GAME OVER", "STACK HIT THE TOP", "PRESS A TO RETRY");
  }

  SDL_RenderPresent(renderer);
}

static void reset_game(int board[BOARD_HEIGHT][BOARD_WIDTH],
                       stonefall_piece* current,
                       int* next_type,
                       int* score,
                       int* level,
                       int* total_lines,
                       int* paused,
                       int* game_over,
                       int* lines_cleared_last) {
  memset(board, 0, sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH);
  *current = make_piece(random_piece_type());
  *next_type = random_piece_type();
  *score = 0;
  *level = 1;
  *total_lines = 0;
  *paused = 0;
  *game_over = 0;
  *lines_cleared_last = 0;
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
  int board[BOARD_HEIGHT][BOARD_WIDTH];
  stonefall_piece current;
  int next_type = 0;
  int score = 0;
  int high_score = 0;
  int level = 1;
  int total_lines = 0;
  int paused = 0;
  int game_over = 0;
  int lines_cleared_last = 0;
  Uint32 last_drop = 0;
  int width = 0;
  int height = 0;

  (void)argc;
  (void)argv;

  memset(&previous_input, 0, sizeof(previous_input));
  memset(&tone, 0, sizeof(tone));
  srand((unsigned int)time(NULL));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (pp_init(&context, "stonefall") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  high_score = load_high_score(&context);
  reset_game(board, &current, &next_type, &score, &level, &total_lines, &paused, &game_over,
             &lines_cleared_last);
  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(512, width);
  height = maximum(512, height);

  window = SDL_CreateWindow("Stonefall", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                            height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

  last_drop = SDL_GetTicks();

  while (!pp_should_exit(&context)) {
    Uint32 now = SDL_GetTicks();
    int drop_interval = 600 - ((level - 1) * 45);
    if (drop_interval < 120) {
      drop_interval = 120;
    }

    pp_poll_input(&context, &input);

    if (input.start && !previous_input.start) {
      if (game_over) {
        reset_game(board, &current, &next_type, &score, &level, &total_lines, &paused,
                   &game_over, &lines_cleared_last);
      } else {
        paused = !paused;
      }
    }

    if (game_over) {
      if ((input.a && !previous_input.a) || (input.start && !previous_input.start)) {
        reset_game(board, &current, &next_type, &score, &level, &total_lines, &paused,
                   &game_over, &lines_cleared_last);
      }
      draw_board(renderer, board, current, paused, game_over, score, high_score, next_type,
                 level, total_lines, lines_cleared_last);
      previous_input = input;
      SDL_Delay(16);
      continue;
    }

    if (!paused) {
      if (input.left && !previous_input.left) {
        stonefall_piece candidate = current;
        candidate.x -= 1;
        if (piece_fits(board, candidate)) {
          current = candidate;
        }
      }

      if (input.right && !previous_input.right) {
        stonefall_piece candidate = current;
        candidate.x += 1;
        if (piece_fits(board, candidate)) {
          current = candidate;
        }
      }

      if ((input.a && !previous_input.a) || (input.up && !previous_input.up)) {
        stonefall_piece candidate = current;
        candidate.rotation = (candidate.rotation + 1) % 4;
        if (piece_fits(board, candidate)) {
          current = candidate;
          trigger_tone(&tone, 880.0f, 40);
        }
      }

      if (input.b && !previous_input.b) {
        stonefall_piece candidate = current;
        while (piece_fits(board, candidate)) {
          current = candidate;
          candidate.y += 1;
        }
        trigger_tone(&tone, 220.0f, 70);
        last_drop = 0;
      }

      if (input.down) {
        drop_interval = 60;
      }

      if (now - last_drop >= (Uint32)drop_interval) {
        stonefall_piece candidate = current;
        candidate.y += 1;
        if (piece_fits(board, candidate)) {
          current = candidate;
        } else {
          int cleared = 0;
          lock_piece(board, current);
          cleared = clear_lines(board);
          lines_cleared_last = cleared;
          score += score_for_lines(cleared);
          total_lines += cleared;
          level = 1 + (total_lines / 6);
          if (cleared > 0) {
            trigger_tone(&tone, 660.0f, 110);
          } else {
            trigger_tone(&tone, 330.0f, 40);
          }
          if (score > high_score) {
            high_score = score;
            save_high_score(&context, high_score);
          }

          current = make_piece(next_type);
          next_type = random_piece_type();
          if (!piece_fits(board, current)) {
            game_over = 1;
            save_high_score(&context, high_score);
            trigger_tone(&tone, 110.0f, 240);
          }
        }
        last_drop = now;
      }
    }

    draw_board(renderer, board, current, paused, game_over, score, high_score, next_type, level,
               total_lines, lines_cleared_last);
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
