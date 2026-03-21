#include "pixelpal/pixelpal.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define BOTBITE_PATH_SEPARATOR '\\'
#else
#define BOTBITE_PATH_SEPARATOR '/'
#endif

#define VIRTUAL_WIDTH 512
#define VIRTUAL_HEIGHT 512
#define MAP_WIDTH 17
#define MAP_HEIGHT 17
#define TILE_SIZE 24
#define BOARD_OFFSET_X 52
#define BOARD_OFFSET_Y 58
#define TUNNEL_ROW 8
#define PLAYER_START_X 8
#define PLAYER_START_Y 14
#define HOME_X 8
#define HOME_Y 9
#define BONUS_X 8
#define BONUS_Y 9
#define RESPAWN_GRACE_MS 900U
#define FRIGHTENED_DURATION_MS 7000U
#define ROBOT_RESPAWN_DELAY_MS 1600U

typedef enum direction {
  DIR_NONE = 0,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_UP,
  DIR_DOWN
} direction;

typedef struct tone_state {
  float phase;
  float frequency;
  int frames_remaining;
} tone_state;

typedef struct actor {
  int x;
  int y;
  int prev_x;
  int prev_y;
  int spawn_x;
  int spawn_y;
  int scatter_x;
  int scatter_y;
  direction dir;
  direction next_dir;
  Uint32 moved_at;
  SDL_Color color;
  int frightened;
  int dead;
  int type;
  Uint32 home_hold_until;
} actor;

static const char* k_maze_template[MAP_HEIGHT] = {
    "#################",
    "#o.....#.#.....o#",
    "#.###.#.#.#.###.#",
    "#...............#",
    "#.###.###.###.#.#",
    "#.....#...#.....#",
    "###.#.#.#.#.#.###",
    "#...#..   ..#...#",
    ".....#.   .#.....",
    "###.#.#   #.#.###",
    "#.......#.......#",
    "#.###.#.#.#.###.#",
    "#o..#.#...#.#..o#",
    "##.#.#.###.#.#.##",
    "#....#.....#....#",
    "#.######.######.#",
    "#################",
};

static const Uint32 k_mode_durations[] = {5000U, 22000U, 4000U, 22000U, 4000U, 0U};
static const int k_mode_is_chase[] = {0, 1, 0, 1, 0, 1};

static const SDL_Color k_bg = {218, 223, 184, 255};
static const SDL_Color k_board_bg = {17, 31, 25, 255};
static const SDL_Color k_board_shadow = {126, 137, 96, 255};
static const SDL_Color k_board_frame = {194, 201, 159, 255};
static const SDL_Color k_wall = {55, 96, 76, 255};
static const SDL_Color k_wall_inner = {84, 138, 108, 255};
static const SDL_Color k_text = {20, 56, 28, 255};
static const SDL_Color k_muted = {70, 101, 59, 255};
static const SDL_Color k_pellet = {235, 239, 211, 255};
static const SDL_Color k_bonus = {250, 220, 138, 255};
static const SDL_Color k_player_body = {232, 183, 82, 255};
static const SDL_Color k_player_skin = {244, 224, 184, 255};
static const SDL_Color k_player_boots = {77, 101, 133, 255};
static const SDL_Color k_frightened = {82, 129, 196, 255};

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

static int maximum(int left, int right) {
  return left > right ? left : right;
}

static int minimum(int left, int right) {
  return left < right ? left : right;
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

static int abs_int(int value) {
  return value < 0 ? -value : value;
}

static float abs_float(float value) {
  return value < 0.0f ? -value : value;
}

static void save_high_score(const pp_context* context, int high_score) {
  char path[PP_PATH_CAPACITY];
  FILE* file = NULL;
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), BOTBITE_PATH_SEPARATOR);
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
  snprintf(path, sizeof(path), "%s%chighscore.txt", pp_get_save_dir(context), BOTBITE_PATH_SEPARATOR);
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
  static const uint8_t slash[7] = {1, 2, 2, 4, 8, 8, 16};
  static const uint8_t period[7] = {0, 0, 0, 0, 0, 12, 12};
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
    case '-': glyph = dash; break; case ':': glyph = colon; break; case '/': glyph = slash; break;
    case '.': glyph = period; break; default: break;
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

static int dir_dx(direction dir) {
  if (dir == DIR_LEFT) {
    return -1;
  }
  if (dir == DIR_RIGHT) {
    return 1;
  }
  return 0;
}

static int dir_dy(direction dir) {
  if (dir == DIR_UP) {
    return -1;
  }
  if (dir == DIR_DOWN) {
    return 1;
  }
  return 0;
}

static direction reverse_direction(direction dir) {
  if (dir == DIR_LEFT) {
    return DIR_RIGHT;
  }
  if (dir == DIR_RIGHT) {
    return DIR_LEFT;
  }
  if (dir == DIR_UP) {
    return DIR_DOWN;
  }
  if (dir == DIR_DOWN) {
    return DIR_UP;
  }
  return DIR_NONE;
}

static void next_tile_coords(int x, int y, direction dir, int* nx, int* ny) {
  int tx = x + dir_dx(dir);
  int ty = y + dir_dy(dir);
  if (ty == TUNNEL_ROW && tx < 0) {
    tx = MAP_WIDTH - 1;
  } else if (ty == TUNNEL_ROW && tx >= MAP_WIDTH) {
    tx = 0;
  }
  *nx = tx;
  *ny = ty;
}

static int is_passable(const char tiles[MAP_HEIGHT][MAP_WIDTH + 1], int x, int y) {
  if (y == TUNNEL_ROW && (x < 0 || x >= MAP_WIDTH)) {
    return 1;
  }
  if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
    return 0;
  }
  return tiles[y][x] != '#';
}

static int can_move_direction(const char tiles[MAP_HEIGHT][MAP_WIDTH + 1], int x, int y, direction dir) {
  int nx = 0;
  int ny = 0;
  if (dir == DIR_NONE) {
    return 0;
  }
  next_tile_coords(x, y, dir, &nx, &ny);
  return is_passable(tiles, nx, ny);
}

static int move_actor(actor* entity,
                      const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                      direction dir,
                      Uint32 moved_at) {
  int nx = 0;
  int ny = 0;
  if (!can_move_direction(tiles, entity->x, entity->y, dir)) {
    return 0;
  }
  entity->prev_x = entity->x;
  entity->prev_y = entity->y;
  next_tile_coords(entity->x, entity->y, dir, &nx, &ny);
  entity->x = nx;
  entity->y = ny;
  entity->dir = dir;
  entity->moved_at = moved_at;
  return 1;
}

static void copy_maze(char tiles[MAP_HEIGHT][MAP_WIDTH + 1], int* pellets_total) {
  int y = 0;
  int total = 0;
  for (y = 0; y < MAP_HEIGHT; ++y) {
    int x = 0;
    memcpy(tiles[y], k_maze_template[y], MAP_WIDTH + 1);
    for (x = 0; x < MAP_WIDTH; ++x) {
      if (tiles[y][x] == '.' || tiles[y][x] == 'o') {
        ++total;
      }
    }
  }
  *pellets_total = total;
}

static void reset_positions(actor* player, actor robots[4]) {
  player->x = PLAYER_START_X;
  player->y = PLAYER_START_Y;
  player->prev_x = PLAYER_START_X;
  player->prev_y = PLAYER_START_Y;
  player->spawn_x = PLAYER_START_X;
  player->spawn_y = PLAYER_START_Y;
  player->dir = DIR_LEFT;
  player->next_dir = DIR_LEFT;
  player->moved_at = 0U;
  player->frightened = 0;
  player->dead = 0;

  robots[0] = (actor){8, 9, 8, 9, 8, 9, MAP_WIDTH - 2, 1, DIR_LEFT, DIR_LEFT, 0U, {240, 120, 120, 255}, 0, 0, 0};
  robots[1] = (actor){7, 9, 7, 9, 7, 9, 1, 1, DIR_RIGHT, DIR_RIGHT, 0U, {240, 165, 193, 255}, 0, 0, 1};
  robots[2] = (actor){9, 9, 9, 9, 9, 9, MAP_WIDTH - 2, MAP_HEIGHT - 2, DIR_LEFT, DIR_LEFT, 0U, {129, 225, 223, 255}, 0, 0, 2};
  robots[3] = (actor){8, 8, 8, 8, 8, 8, 1, MAP_HEIGHT - 2, DIR_RIGHT, DIR_RIGHT, 0U, {250, 184, 104, 255}, 0, 0, 3};
}

static void apply_easy_power_pellets(char tiles[MAP_HEIGHT][MAP_WIDTH + 1], int level) {
  if (level > 3) {
    return;
  }
  if (tiles[7][1] == '.') {
    tiles[7][1] = 'o';
  }
  if (tiles[7][15] == '.') {
    tiles[7][15] = 'o';
  }
}

static void start_stage(char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                        int* pellets_remaining,
                        int* pellets_total,
                        actor* player,
                        actor robots[4],
                        int* frightened_active,
                        Uint32* frightened_until,
                        int* robot_streak,
                        int* bonus_active,
                        int* bonus_spawned,
                        Uint32* bonus_started_at,
                        int* chase_mode,
                        int* phase_index,
                        Uint32* phase_started_at,
                        int level,
                        Uint32 now) {
  copy_maze(tiles, pellets_total);
  apply_easy_power_pellets(tiles, level);
  *pellets_remaining = *pellets_total;
  *frightened_active = 0;
  *frightened_until = 0U;
  *robot_streak = 0;
  *bonus_active = 0;
  *bonus_spawned = 0;
  *bonus_started_at = 0U;
  *chase_mode = 0;
  *phase_index = 0;
  *phase_started_at = now;
  reset_positions(player, robots);
}

static void reset_after_life(actor* player,
                             actor robots[4],
                             int* frightened_active,
                             Uint32* frightened_until,
                             int* robot_streak,
                             int* bonus_active,
                             int* chase_mode,
                             int* phase_index,
                             Uint32* phase_started_at,
                             Uint32 now) {
  *frightened_active = 0;
  *frightened_until = 0U;
  *robot_streak = 0;
  *bonus_active = 0;
  *chase_mode = 0;
  *phase_index = 0;
  *phase_started_at = now;
  reset_positions(player, robots);
}

static void update_mode_phase(int* chase_mode, int* phase_index, Uint32* phase_started_at, Uint32 now) {
  while (k_mode_durations[*phase_index] > 0U &&
         now - *phase_started_at >= k_mode_durations[*phase_index] &&
         *phase_index < 5) {
    ++(*phase_index);
    *phase_started_at = now;
  }
  *chase_mode = k_mode_is_chase[*phase_index];
}

static void player_target_ahead(const actor* player, int steps, int* tx, int* ty) {
  int x = player->x;
  int y = player->y;
  int count = 0;
  for (count = 0; count < steps; ++count) {
    x += dir_dx(player->dir);
    y += dir_dy(player->dir);
    if (y == TUNNEL_ROW && x < 0) {
      x = MAP_WIDTH - 1;
    } else if (y == TUNNEL_ROW && x >= MAP_WIDTH) {
      x = 0;
    }
  }
  *tx = x;
  *ty = y;
}

static void normalize_target_coords(int* tx, int* ty) {
  if (*ty == TUNNEL_ROW) {
    if (*tx < 0) {
      *tx = MAP_WIDTH - 1;
    } else if (*tx >= MAP_WIDTH) {
      *tx = 0;
    }
  }
  *tx = clamp_int(*tx, 0, MAP_WIDTH - 1);
  *ty = clamp_int(*ty, 0, MAP_HEIGHT - 1);
}

static int clear_path_to_player(const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                                const actor* robot,
                                const actor* player) {
  if (robot->x == player->x) {
    int start = minimum(robot->y, player->y) + 1;
    int end = maximum(robot->y, player->y);
    int y = 0;
    for (y = start; y < end; ++y) {
      if (tiles[y][robot->x] == '#') {
        return 0;
      }
    }
    return 1;
  }
  if (robot->y == player->y) {
    int start = minimum(robot->x, player->x) + 1;
    int end = maximum(robot->x, player->x);
    int x = 0;
    for (x = start; x < end; ++x) {
      if (tiles[robot->y][x] == '#') {
        return 0;
      }
    }
    return 1;
  }
  return 0;
}

static void target_for_robot(const actor* robot,
                             const actor* player,
                             const actor robots[4],
                             const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                             int chase_mode,
                             int* tx,
                             int* ty) {
  if (robot->dead) {
    *tx = HOME_X;
    *ty = HOME_Y;
    return;
  }
  if (!chase_mode) {
    *tx = robot->scatter_x;
    *ty = robot->scatter_y;
    return;
  }
  if (clear_path_to_player(tiles, robot, player) ||
      abs(robot->x - player->x) + abs(robot->y - player->y) <= 4) {
    *tx = player->x;
    *ty = player->y;
    return;
  }
  if (robot->type == 0) {
    *tx = player->x;
    *ty = player->y;
    return;
  }
  if (robot->type == 1) {
    player_target_ahead(player, 4, tx, ty);
    return;
  }
  if (robot->type == 2) {
    int ahead_x = 0;
    int ahead_y = 0;
    player_target_ahead(player, 2, &ahead_x, &ahead_y);
    *tx = ahead_x + (ahead_x - robots[0].x);
    *ty = ahead_y + (ahead_y - robots[0].y);
    normalize_target_coords(tx, ty);
    return;
  }
  if (abs(robot->x - player->x) + abs(robot->y - player->y) > 6) {
    *tx = player->x;
    *ty = player->y;
  } else {
    *tx = robot->scatter_x;
    *ty = robot->scatter_y;
  }
  normalize_target_coords(tx, ty);
}

static int route_score_to_target(const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                                 int start_x,
                                 int start_y,
                                 int target_x,
                                 int target_y) {
  int queue_x[MAP_WIDTH * MAP_HEIGHT];
  int queue_y[MAP_WIDTH * MAP_HEIGHT];
  int queue_dist[MAP_WIDTH * MAP_HEIGHT];
  int visited[MAP_HEIGHT][MAP_WIDTH];
  int head = 0;
  int tail = 0;
  int best_score = 1 << 30;
  int y = 0;

  normalize_target_coords(&target_x, &target_y);
  for (y = 0; y < MAP_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < MAP_WIDTH; ++x) {
      visited[y][x] = 0;
    }
  }

  if (!is_passable(tiles, start_x, start_y)) {
    return best_score;
  }

  queue_x[tail] = start_x;
  queue_y[tail] = start_y;
  queue_dist[tail] = 0;
  visited[start_y][start_x] = 1;
  ++tail;

  while (head < tail) {
    int x = queue_x[head];
    int current_y = queue_y[head];
    int dist = queue_dist[head];
    int dx = abs_int(target_x - x);
    int heuristic = dx * dx + abs_int(target_y - current_y) * abs_int(target_y - current_y);
    int score = dist * 32 + heuristic;
    int dir_index = 0;
    if (score < best_score) {
      best_score = score;
    }
    if (x == target_x && current_y == target_y) {
      return dist * 32;
    }
    ++head;

    for (dir_index = 0; dir_index < 4; ++dir_index) {
      static const direction search_dirs[4] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};
      int nx = 0;
      int ny = 0;
      next_tile_coords(x, current_y, search_dirs[dir_index], &nx, &ny);
      if (!is_passable(tiles, nx, ny) || visited[ny][nx]) {
        continue;
      }
      visited[ny][nx] = 1;
      queue_x[tail] = nx;
      queue_y[tail] = ny;
      queue_dist[tail] = dist + 1;
      ++tail;
    }
  }

  return best_score;
}

static direction choose_robot_direction(const actor* robot,
                                        const actor* player,
                                        const actor robots[4],
                                        const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                                        int chase_mode) {
  direction candidates[4] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};
  direction valid[4];
  direction reverse = reverse_direction(robot->dir);
  direction best_dir = robot->dir;
  int valid_count = 0;
  int best_score = 0;
  int best_set = 0;
  int target_x = 0;
  int target_y = 0;
  int index = 0;

  for (index = 0; index < 4; ++index) {
    direction dir = candidates[index];
    if (!can_move_direction(tiles, robot->x, robot->y, dir) || dir == reverse) {
      continue;
    }
    valid[valid_count++] = dir;
  }
  if (valid_count == 0) {
    for (index = 0; index < 4; ++index) {
      direction dir = candidates[index];
      if (can_move_direction(tiles, robot->x, robot->y, dir)) {
        valid[valid_count++] = dir;
      }
    }
  }
  if (valid_count == 0) {
    return DIR_NONE;
  }
  if (robot->frightened && !robot->dead) {
    if (valid_count == 1) {
      return valid[0];
    }
    if (robot->dir != DIR_NONE && can_move_direction(tiles, robot->x, robot->y, robot->dir) &&
        rand() % 100 < 45) {
      return robot->dir;
    }
    return valid[rand() % valid_count];
  }

  target_for_robot(robot, player, robots, tiles, chase_mode, &target_x, &target_y);
  for (index = 0; index < valid_count; ++index) {
    int nx = 0;
    int ny = 0;
    int score = 0;
    int turn_penalty = 0;
    next_tile_coords(robot->x, robot->y, valid[index], &nx, &ny);
    score = route_score_to_target(tiles, nx, ny, target_x, target_y);
    turn_penalty = (valid[index] == robot->dir) ? 0 : 1;
    score = score * 4 + turn_penalty;
    if (!best_set || score < best_score) {
      best_score = score;
      best_dir = valid[index];
      best_set = 1;
    }
  }
  return best_dir;
}

static void set_player_requested_direction(actor* player, const pp_input_state* input) {
  if (input->left) {
    player->next_dir = DIR_LEFT;
  } else if (input->right) {
    player->next_dir = DIR_RIGHT;
  } else if (input->up) {
    player->next_dir = DIR_UP;
  } else if (input->down) {
    player->next_dir = DIR_DOWN;
  }
}

static void update_player(actor* player,
                          const pp_input_state* input,
                          const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                          Uint32 now) {
  set_player_requested_direction(player, input);
  if (player->next_dir != DIR_NONE &&
      can_move_direction(tiles, player->x, player->y, player->next_dir)) {
    player->dir = player->next_dir;
  }
  if (player->dir != DIR_NONE) {
    move_actor(player, tiles, player->dir, now);
  }
}

static void update_robots(actor robots[4],
                          const actor* player,
                          const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                          int chase_mode,
                          Uint32 now) {
  int index = 0;
  for (index = 0; index < 4; ++index) {
    actor* robot = &robots[index];
    if (robot->dead && robot->x == HOME_X && robot->y == HOME_Y) {
      if (robot->home_hold_until == 0U) {
        robot->home_hold_until = now + ROBOT_RESPAWN_DELAY_MS;
      }
      if (now < robot->home_hold_until) {
        continue;
      }
      robot->dead = 0;
      robot->frightened = 0;
      robot->home_hold_until = 0U;
    }
    robot->next_dir = choose_robot_direction(robot, player, robots, tiles, chase_mode);
    if (robot->next_dir != DIR_NONE) {
      move_actor(robot, tiles, robot->next_dir, now);
    }
  }
}

static void interpolated_actor_position(const actor* entity,
                                        Uint32 now,
                                        int interval,
                                        float* tile_x,
                                        float* tile_y) {
  float start_x = (float)entity->prev_x;
  float start_y = (float)entity->prev_y;
  float end_x = (float)entity->x;
  float end_y = (float)entity->y;
  float t = 1.0f;

  if (interval > 0 && entity->moved_at != 0U &&
      (entity->prev_x != entity->x || entity->prev_y != entity->y)) {
    t = (float)(now - entity->moved_at) / (float)interval;
    if (t < 0.0f) {
      t = 0.0f;
    }
    if (t > 1.0f) {
      t = 1.0f;
    }
    if (entity->prev_y == TUNNEL_ROW && entity->y == TUNNEL_ROW &&
        abs(entity->x - entity->prev_x) > 1) {
      if (entity->dir == DIR_LEFT) {
        end_x = -1.0f;
      } else if (entity->dir == DIR_RIGHT) {
        end_x = (float)MAP_WIDTH;
      }
    }
  }

  *tile_x = start_x + (end_x - start_x) * t;
  *tile_y = start_y + (end_y - start_y) * t;
  if (*tile_x < 0.0f) {
    *tile_x += (float)MAP_WIDTH;
  } else if (*tile_x >= (float)MAP_WIDTH) {
    *tile_x -= (float)MAP_WIDTH;
  }
}

static float wrapped_x_delta(float delta) {
  const float half_width = (float)MAP_WIDTH * 0.5f;
  if (delta > half_width) {
    delta -= (float)MAP_WIDTH;
  } else if (delta < -half_width) {
    delta += (float)MAP_WIDTH;
  }
  return delta;
}

static int actors_overlap_now(const actor* player,
                              int player_interval,
                              const actor* robot,
                              int robot_interval,
                              Uint32 now) {
  float player_x = 0.0f;
  float player_y = 0.0f;
  float robot_x = 0.0f;
  float robot_y = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;
  const float hitbox_x = 0.34f;
  const float hitbox_y = 0.40f;

  interpolated_actor_position(player, now, player_interval, &player_x, &player_y);
  interpolated_actor_position(robot, now, robot_interval, &robot_x, &robot_y);
  dx = wrapped_x_delta(player_x - robot_x);
  dy = player_y - robot_y;
  return abs_float(dx) <= hitbox_x && abs_float(dy) <= hitbox_y;
}

static int resolve_collisions(actor* player,
                              actor robots[4],
                              int* score,
                              int* high_score,
                              int* robot_streak,
                              tone_state* tone,
                              Uint32 now,
                              int player_interval,
                              int robot_interval) {
  static const int eat_scores[4] = {200, 400, 800, 1600};
  int index = 0;
  for (index = 0; index < 4; ++index) {
    actor* robot = &robots[index];
    if (robot->dead ||
        !actors_overlap_now(player, player_interval, robot, robot_interval, now)) {
      continue;
    }
    if (robot->frightened) {
      *score += eat_scores[minimum(*robot_streak, 3)];
      *robot_streak = minimum(*robot_streak + 1, 3);
      if (*score > *high_score) {
        *high_score = *score;
      }
      robot->dead = 1;
      robot->frightened = 0;
      robot->dir = reverse_direction(robot->dir);
      robot->home_hold_until = 0U;
      trigger_tone(tone, 980.0f, 120);
      continue;
    }
    return 1;
  }
  return 0;
}

static void draw_wall_tile(SDL_Renderer* renderer, int x, int y) {
  SDL_Rect outer = {BOARD_OFFSET_X + x * TILE_SIZE, BOARD_OFFSET_Y + y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
  SDL_Rect inner = {outer.x + 4, outer.y + 4, TILE_SIZE - 8, TILE_SIZE - 8};
  fill_rect(renderer, outer, k_wall);
  fill_rect(renderer, inner, k_wall_inner);
}

static void draw_repairman(SDL_Renderer* renderer,
                           const actor* player,
                           Uint32 ticks,
                           Uint32 now,
                           int player_interval) {
  float tile_x = 0.0f;
  float tile_y = 0.0f;
  int px = 0;
  int py = 0;
  int facing = 1;
  int arm_offset = 0;
  int hammer_head_x = 0;
  interpolated_actor_position(player, now, player_interval, &tile_x, &tile_y);
  px = BOARD_OFFSET_X + (int)(tile_x * TILE_SIZE) + TILE_SIZE / 2;
  py = BOARD_OFFSET_Y + (int)(tile_y * TILE_SIZE) + TILE_SIZE / 2;
  if (player->dir == DIR_LEFT || (player->dir == DIR_NONE && player->next_dir == DIR_LEFT)) {
    facing = -1;
  }
  arm_offset = (((ticks / 140U) % 2U == 0U) ? -2 : 2) * facing;
  hammer_head_x = px + arm_offset + (facing * 8);
  fill_circle(renderer, px, py - 6, 5, k_player_skin);
  fill_rect(renderer, (SDL_Rect){px - 5, py - 1, 10, 11}, k_player_body);
  fill_rect(renderer, (SDL_Rect){px - 6, py + 10, 4, 5}, k_player_boots);
  fill_rect(renderer, (SDL_Rect){px + 2, py + 10, 4, 5}, k_player_boots);
  fill_rect(renderer, (SDL_Rect){px + arm_offset, py - 2, 9, 3}, k_player_skin);
  fill_rect(renderer, (SDL_Rect){hammer_head_x, py - 3, 4, 6}, k_bonus);
}

static void draw_robot(SDL_Renderer* renderer,
                       const actor* robot,
                       Uint32 ticks,
                       Uint32 now,
                       int robot_interval) {
  SDL_Color body = robot->frightened ? k_frightened : robot->color;
  float tile_x = 0.0f;
  float tile_y = 0.0f;
  int px = 0;
  int py = 0;
  interpolated_actor_position(robot, now, robot_interval, &tile_x, &tile_y);
  px = BOARD_OFFSET_X + (int)(tile_x * TILE_SIZE) + TILE_SIZE / 2;
  py = BOARD_OFFSET_Y + (int)(tile_y * TILE_SIZE) + TILE_SIZE / 2;
  int blink = ((ticks / 180U) % 2U == 0U) ? 0 : 1;
  int eye_dx = 0;
  int eye_dy = 0;
  if (robot->dead) {
    fill_rect(renderer, (SDL_Rect){px - 5, py - 2, 4, 4}, k_pellet);
    fill_rect(renderer, (SDL_Rect){px + 1, py - 2, 4, 4}, k_pellet);
    return;
  }
  if (robot->dir == DIR_LEFT) {
    eye_dx = -1;
  } else if (robot->dir == DIR_RIGHT) {
    eye_dx = 1;
  } else if (robot->dir == DIR_UP) {
    eye_dy = -1;
  } else if (robot->dir == DIR_DOWN) {
    eye_dy = 1;
  }
  fill_rect(renderer, (SDL_Rect){px - 6, py - 5, 12, 12}, body);
  fill_rect(renderer, (SDL_Rect){px - 4, py + 7, 2, 4}, body);
  fill_rect(renderer, (SDL_Rect){px, py + 7, 2, 4}, body);
  fill_rect(renderer, (SDL_Rect){px + 4, py + 7, 2, 4}, body);
  fill_rect(renderer, (SDL_Rect){px - 1, py - 9, 2, 4}, body);
  fill_rect(renderer, (SDL_Rect){px - 3, py - 11, 6, 2}, body);
  fill_rect(renderer, (SDL_Rect){px - 4, py - 3, 3, 3}, k_pellet);
  fill_rect(renderer, (SDL_Rect){px + 1, py - 3, 3, 3}, k_pellet);
  fill_rect(renderer, (SDL_Rect){px - 3 + blink + eye_dx, py - 2 + eye_dy, 1, 1}, k_text);
  fill_rect(renderer, (SDL_Rect){px + 2 + blink + eye_dx, py - 2 + eye_dy, 1, 1}, k_text);
}

static void draw_maze(SDL_Renderer* renderer,
                      const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                      int bonus_active,
                      Uint32 ticks) {
  int y = 0;
  for (y = 0; y < MAP_HEIGHT; ++y) {
    int x = 0;
    for (x = 0; x < MAP_WIDTH; ++x) {
      const char tile = tiles[y][x];
      if (tile == '#') {
        draw_wall_tile(renderer, x, y);
      } else if (tile == '.') {
        fill_circle(renderer, BOARD_OFFSET_X + x * TILE_SIZE + TILE_SIZE / 2,
                    BOARD_OFFSET_Y + y * TILE_SIZE + TILE_SIZE / 2, 2, k_pellet);
      } else if (tile == 'o') {
        int radius = ((ticks / 220U) % 2U == 0U) ? 5 : 4;
        fill_circle(renderer, BOARD_OFFSET_X + x * TILE_SIZE + TILE_SIZE / 2,
                    BOARD_OFFSET_Y + y * TILE_SIZE + TILE_SIZE / 2, radius, k_bonus);
      }
    }
  }
  if (bonus_active) {
    fill_rect(renderer,
              (SDL_Rect){BOARD_OFFSET_X + BONUS_X * TILE_SIZE + 7,
                         BOARD_OFFSET_Y + BONUS_Y * TILE_SIZE + 7, 10, 10},
              k_bonus);
  }
}

static void draw_header(SDL_Renderer* renderer,
                        int score,
                        int high_score,
                        int level,
                        int lives,
                        int frightened_active) {
  char buffer[64];
  draw_text(renderer, "BOTBYTE", VIRTUAL_WIDTH / 2, 10, 4, k_text, 1);
  snprintf(buffer, sizeof(buffer), "SCORE %d", score);
  draw_text(renderer, buffer, 18, 22, 2, k_text, 0);
  snprintf(buffer, sizeof(buffer), "BEST %d", high_score);
  draw_text_right(renderer, buffer, VIRTUAL_WIDTH - 18, 22, 2, k_text);
  snprintf(buffer, sizeof(buffer), "LEVEL %d", level);
  draw_text(renderer, buffer, 18, 482, 2, k_text, 0);
  snprintf(buffer, sizeof(buffer), "LIVES %d", lives);
  draw_text_right(renderer, buffer, VIRTUAL_WIDTH - 18, 482, 2, k_text);
  if (frightened_active) {
    draw_text(renderer, "EMP HOT", VIRTUAL_WIDTH / 2, 44, 1, k_muted, 1);
  } else {
    draw_text(renderer, "D PAD MOVE  START PAUSE", VIRTUAL_WIDTH / 2, 44, 1, k_muted, 1);
  }
}

static void draw_overlay(SDL_Renderer* renderer, const char* title, const char* subtitle, const char* prompt) {
  SDL_Rect box = {84, 188, 344, 112};
  fill_rect(renderer, box, (SDL_Color){25, 45, 34, 228});
  stroke_rect(renderer, box, k_board_frame);
  draw_text(renderer, title, VIRTUAL_WIDTH / 2, 204, 4, k_pellet, 1);
  draw_text(renderer, subtitle, VIRTUAL_WIDTH / 2, 244, 2, k_pellet, 1);
  draw_text(renderer, prompt, VIRTUAL_WIDTH / 2, 270, 2, k_pellet, 1);
}

static void render_scene(SDL_Renderer* renderer,
                         const char tiles[MAP_HEIGHT][MAP_WIDTH + 1],
                         const actor* player,
                         const actor robots[4],
                         int score,
                         int high_score,
                         int level,
                         int lives,
                         int frightened_active,
                         int bonus_active,
                         int paused,
                         int game_over,
                         Uint32 ticks,
                         int player_interval,
                         int robot_interval) {
  SDL_Rect body = {0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT};
  SDL_Rect shadow = {BOARD_OFFSET_X - 10, BOARD_OFFSET_Y - 10, MAP_WIDTH * TILE_SIZE + 20, MAP_HEIGHT * TILE_SIZE + 20};
  SDL_Rect board = {BOARD_OFFSET_X - 14, BOARD_OFFSET_Y - 14, MAP_WIDTH * TILE_SIZE + 28, MAP_HEIGHT * TILE_SIZE + 28};
  int index = 0;

  fill_rect(renderer, body, k_bg);
  fill_rect(renderer, shadow, k_board_shadow);
  fill_rect(renderer, board, k_board_frame);
  fill_rect(renderer, (SDL_Rect){BOARD_OFFSET_X, BOARD_OFFSET_Y, MAP_WIDTH * TILE_SIZE, MAP_HEIGHT * TILE_SIZE}, k_board_bg);
  draw_header(renderer, score, high_score, level, lives, frightened_active);
  draw_maze(renderer, tiles, bonus_active, ticks);
  draw_repairman(renderer, player, ticks, ticks, player_interval);
  for (index = 0; index < 4; ++index) {
    draw_robot(renderer, &robots[index], ticks, ticks, robot_interval);
  }
  draw_text(renderer, "HOLD START SELECT TO EXIT", VIRTUAL_WIDTH / 2, 500, 1, k_muted, 1);
  if (paused) {
    draw_overlay(renderer, "PAUSED", "TOOLS DOWN", "PRESS START TO RESUME");
  }
  if (game_over) {
    draw_overlay(renderer, "GAME OVER", "ROGUE BOTS WON", "PRESS A OR START");
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
  char tiles[MAP_HEIGHT][MAP_WIDTH + 1];
  actor player;
  actor robots[4];
  int pellets_remaining = 0;
  int pellets_total = 0;
  int lives = 3;
  int score = 0;
  int high_score = 0;
  int level = 1;
  int paused = 0;
  int game_over = 0;
  int frightened_active = 0;
  int robot_streak = 0;
  int chase_mode = 0;
  int phase_index = 0;
  int bonus_active = 0;
  int bonus_spawned = 0;
  Uint32 frightened_until = 0U;
  Uint32 phase_started_at = 0U;
  Uint32 bonus_started_at = 0U;
  Uint32 last_player_step = 0U;
  Uint32 last_robot_step = 0U;
  Uint32 respawn_safe_until = 0U;
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
  if (pp_init(&context, "botbyte") != 0) {
    fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  pp_get_framebuffer_size(&context, &width, &height);
  width = maximum(512, width);
  height = maximum(512, height);
  high_score = load_high_score(&context);

  start_stage(tiles, &pellets_remaining, &pellets_total, &player, robots, &frightened_active,
              &frightened_until, &robot_streak, &bonus_active, &bonus_spawned,
              &bonus_started_at, &chase_mode, &phase_index, &phase_started_at, level, SDL_GetTicks());

  window = SDL_CreateWindow("BotByte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
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

  last_player_step = SDL_GetTicks();
  last_robot_step = SDL_GetTicks();
  respawn_safe_until = last_player_step + RESPAWN_GRACE_MS;
  while (!pp_should_exit(&context)) {
    Uint32 now = SDL_GetTicks();
    int player_interval = maximum(80, 161 - (level - 1) * 8);
    int robot_interval = frightened_active ? maximum(122, 204 - (level - 1) * 5)
                                           : maximum(102, 181 - (level - 1) * 7);

    pp_poll_input(&context, &input);
    if (input.start && !previous_input.start) {
      if (game_over) {
        lives = 3;
        score = 0;
        level = 1;
        game_over = 0;
        start_stage(tiles, &pellets_remaining, &pellets_total, &player, robots, &frightened_active,
                    &frightened_until, &robot_streak, &bonus_active, &bonus_spawned,
                    &bonus_started_at, &chase_mode, &phase_index, &phase_started_at, level, now);
        last_player_step = now;
        last_robot_step = now;
        respawn_safe_until = now + RESPAWN_GRACE_MS;
        trigger_tone(&tone, 660.0f, 80);
      } else {
        paused = !paused;
        trigger_tone(&tone, paused ? 330.0f : 660.0f, 45);
      }
    }

    if (game_over) {
      if ((input.a && !previous_input.a) || (input.b && !previous_input.b)) {
        lives = 3;
        score = 0;
        level = 1;
        game_over = 0;
        start_stage(tiles, &pellets_remaining, &pellets_total, &player, robots, &frightened_active,
                    &frightened_until, &robot_streak, &bonus_active, &bonus_spawned,
                    &bonus_started_at, &chase_mode, &phase_index, &phase_started_at, level, now);
        last_player_step = now;
        last_robot_step = now;
        respawn_safe_until = now + RESPAWN_GRACE_MS;
        trigger_tone(&tone, 660.0f, 80);
      }
      render_scene(renderer, tiles, &player, robots, score, high_score, level, lives,
                   frightened_active, bonus_active, paused, game_over, now,
                   player_interval, robot_interval);
      previous_input = input;
      SDL_Delay(16);
      continue;
    }

    if (!paused) {
      if (frightened_active && now >= frightened_until) {
        int index = 0;
        frightened_active = 0;
        robot_streak = 0;
        for (index = 0; index < 4; ++index) {
          if (!robots[index].dead) {
            robots[index].frightened = 0;
          }
        }
      }

      update_mode_phase(&chase_mode, &phase_index, &phase_started_at, now);
      if (!bonus_spawned && pellets_remaining <= pellets_total / 2) {
        bonus_active = 1;
        bonus_spawned = 1;
        bonus_started_at = now;
      }
      if (bonus_active && now - bonus_started_at >= 9000U) {
        bonus_active = 0;
      }

      if (now - last_player_step >= (Uint32)player_interval) {
        update_player(&player, &input, tiles, now);
        last_player_step = now;
        if (tiles[player.y][player.x] == '.') {
          tiles[player.y][player.x] = ' ';
          --pellets_remaining;
          score += 10;
          trigger_tone(&tone, 720.0f, 18);
        } else if (tiles[player.y][player.x] == 'o') {
          int index = 0;
          Uint32 frightened_base = frightened_active ? maximum((int)frightened_until, (int)now) : now;
          tiles[player.y][player.x] = ' ';
          --pellets_remaining;
          score += 50;
          frightened_active = 1;
          frightened_until = frightened_base + FRIGHTENED_DURATION_MS;
          robot_streak = 0;
          for (index = 0; index < 4; ++index) {
            if (!robots[index].dead) {
              robots[index].frightened = 1;
            }
          }
          trigger_tone(&tone, 460.0f, 110);
        }
        if (bonus_active && player.x == BONUS_X && player.y == BONUS_Y) {
          bonus_active = 0;
          score += 500;
          trigger_tone(&tone, 1180.0f, 130);
        }
        if (score > high_score) {
          high_score = score;
          save_high_score(&context, high_score);
        }
      }

      if (!game_over && now - last_robot_step >= (Uint32)robot_interval) {
        update_robots(robots, &player, tiles, chase_mode, now);
        last_robot_step = now;
      }

      if (!game_over && now >= respawn_safe_until &&
          resolve_collisions(&player, robots, &score, &high_score, &robot_streak, &tone,
                             now, player_interval, robot_interval)) {
        lives -= 1;
        if (lives <= 0) {
          game_over = 1;
          paused = 0;
          trigger_tone(&tone, 140.0f, 260);
          save_high_score(&context, high_score);
        } else {
          reset_after_life(&player, robots, &frightened_active, &frightened_until,
                           &robot_streak, &bonus_active, &chase_mode, &phase_index,
                           &phase_started_at, now);
          last_player_step = now;
          last_robot_step = now;
          respawn_safe_until = now + RESPAWN_GRACE_MS;
          trigger_tone(&tone, 220.0f, 160);
        }
      }

      if (!game_over && pellets_remaining <= 0) {
        level += 1;
        start_stage(tiles, &pellets_remaining, &pellets_total, &player, robots, &frightened_active,
                    &frightened_until, &robot_streak, &bonus_active, &bonus_spawned,
                    &bonus_started_at, &chase_mode, &phase_index, &phase_started_at, level, now);
        last_player_step = now;
        last_robot_step = now;
        respawn_safe_until = now + RESPAWN_GRACE_MS;
        trigger_tone(&tone, 900.0f, 180);
      }
    }

    render_scene(renderer, tiles, &player, robots, score, high_score, level, lives,
                 frightened_active, bonus_active, paused, game_over, now,
                 player_interval, robot_interval);
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
