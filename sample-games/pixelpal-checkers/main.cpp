#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kWindowWidth = 512;
constexpr int kWindowHeight = 512;
constexpr int kBoardSize = 8;
constexpr int kCellSize = 40;
constexpr int kBoardX = 24;
constexpr int kBoardY = 96;
constexpr int kPanelX = 350;
constexpr int kPanelY = 96;
constexpr int kPanelW = 138;
constexpr int kPanelH = 320;
constexpr int kSearchDepth = 7;
constexpr int kAiDelayMs = 220;

enum Piece {
  kEmpty = 0,
  kWhiteMan = 1,
  kWhiteKing = 2,
  kBlackMan = -1,
  kBlackKing = -2,
};

using Board = std::array<std::array<int, kBoardSize>, kBoardSize>;

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Move {
  int from_x = 0;
  int from_y = 0;
  std::vector<std::pair<int, int>> path;
  std::vector<std::pair<int, int>> captures;
  bool promotes = false;
};

struct Theme {
  SDL_Color background{19, 28, 24, 255};
  SDL_Color frame{48, 88, 73, 255};
  SDL_Color panel{29, 46, 39, 255};
  SDL_Color panel_highlight{43, 71, 60, 255};
  SDL_Color light_square{189, 211, 178, 255};
  SDL_Color dark_square{74, 117, 91, 255};
  SDL_Color cursor{248, 222, 127, 255};
  SDL_Color text{226, 242, 212, 255};
  SDL_Color muted{135, 170, 145, 255};
  SDL_Color white_piece{233, 243, 232, 255};
  SDL_Color black_piece{41, 48, 44, 255};
  SDL_Color king_ring{255, 200, 110, 255};
  SDL_Color move_hint{121, 240, 218, 255};
  SDL_Color overlay{8, 13, 12, 218};
};

struct GameState {
  Board board{};
  bool white_turn = true;
  bool paused = false;
  bool game_over = false;
  int winner = 0;
  int cursor_x = 1;
  int cursor_y = 5;
  int selected_x = -1;
  int selected_y = -1;
  std::vector<Move> legal_moves;
  Uint32 ai_ready_at = 0U;
  bool versus_cpu = true;
  bool mode_select_active = true;
  int mode_index = 0;
};

bool is_dark_square(int x, int y) {
  return ((x + y) % 2) == 1;
}

bool is_white_piece(int piece) {
  return piece > 0;
}

bool is_black_piece(int piece) {
  return piece < 0;
}

bool is_king_piece(int piece) {
  return piece == kWhiteKing || piece == kBlackKing;
}

int sign(int value) {
  if (value < 0) {
    return -1;
  }
  if (value > 0) {
    return 1;
  }
  return 0;
}

bool in_bounds(int x, int y) {
  return x >= 0 && x < kBoardSize && y >= 0 && y < kBoardSize;
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void fill_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int dy = -radius; dy <= radius; ++dy) {
    const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    SDL_RenderDrawLine(renderer, cx - span, cy + dy, cx + span, cy + dy);
  }
}

std::array<uint8_t, 7> glyph_for(char ch) {
  switch (ch) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D': return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
}

int text_width(const std::string& text, int scale) {
  if (text.empty()) {
    return 0;
  }
  return static_cast<int>(text.size()) * (6 * scale) - scale;
}

void draw_text(SDL_Renderer* renderer,
               const std::string& text,
               int x,
               int y,
               int scale,
               SDL_Color color,
               bool centered) {
  const std::string upper = uppercase(text);
  int draw_x = x;
  if (centered) {
    draw_x -= text_width(upper, scale) / 2;
  }
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char ch : upper) {
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[row] & (1 << (4 - col))) == 0) {
          continue;
        }
        SDL_Rect pixel{draw_x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    draw_x += 6 * scale;
  }
}

void draw_text_right(SDL_Renderer* renderer,
                     const std::string& text,
                     int right_x,
                     int y,
                     int scale,
                     SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, false);
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  auto* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = (tone->phase < 3.14159f) ? 1500 : -1500;
      tone->phase += (6.28318f * tone->frequency) / 48000.0f;
      if (tone->phase >= 6.28318f) {
        tone->phase -= 6.28318f;
      }
      --tone->frames_remaining;
    }
    samples[index] = sample;
  }
}

void trigger_tone(ToneState& tone, float frequency, int milliseconds) {
  tone.frequency = frequency;
  tone.frames_remaining = (48000 * milliseconds) / 1000;
}

void setup_board(Board& board) {
  for (int y = 0; y < kBoardSize; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
      board[y][x] = kEmpty;
      if (!is_dark_square(x, y)) {
        continue;
      }
      if (y < 3) {
        board[y][x] = kBlackMan;
      } else if (y > 4) {
        board[y][x] = kWhiteMan;
      }
    }
  }
}

std::vector<std::pair<int, int>> movement_directions(int piece, bool capture_mode) {
  const bool king = is_king_piece(piece);
  const int forward = piece > 0 ? -1 : 1;
  if (king) {
    return {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  }
  if (capture_mode) {
    return {{-1, forward}, {1, forward}};
  }
  return {{-1, forward}, {1, forward}};
}

void recurse_captures(const Board& board,
                      int x,
                      int y,
                      int piece,
                      Move current,
                      std::vector<Move>& results) {
  bool found = false;
  for (const auto& dir : movement_directions(piece, true)) {
    const int mid_x = x + dir.first;
    const int mid_y = y + dir.second;
    const int land_x = x + dir.first * 2;
    const int land_y = y + dir.second * 2;
    if (!in_bounds(mid_x, mid_y) || !in_bounds(land_x, land_y)) {
      continue;
    }
    const int mid_piece = board[mid_y][mid_x];
    if (mid_piece == kEmpty || sign(mid_piece) == sign(piece) || board[land_y][land_x] != kEmpty) {
      continue;
    }

    Board next = board;
    next[y][x] = kEmpty;
    next[mid_y][mid_x] = kEmpty;
    int next_piece = piece;
    bool promotes = false;
    if (piece == kWhiteMan && land_y == 0) {
      next_piece = kWhiteKing;
      promotes = true;
    } else if (piece == kBlackMan && land_y == kBoardSize - 1) {
      next_piece = kBlackKing;
      promotes = true;
    }
    next[land_y][land_x] = next_piece;

    Move extended = current;
    extended.path.push_back({land_x, land_y});
    extended.captures.push_back({mid_x, mid_y});
    extended.promotes = promotes;
    found = true;
    if (promotes) {
      results.push_back(extended);
    } else {
      recurse_captures(next, land_x, land_y, next_piece, extended, results);
    }
  }

  if (!found && !current.captures.empty()) {
    results.push_back(current);
  }
}

std::vector<Move> generate_piece_moves(const Board& board, int x, int y, bool capture_only) {
  std::vector<Move> moves;
  const int piece = board[y][x];
  if (piece == kEmpty) {
    return moves;
  }

  Move seed;
  seed.from_x = x;
  seed.from_y = y;
  recurse_captures(board, x, y, piece, seed, moves);
  if (!moves.empty() || capture_only) {
    return moves;
  }

  for (const auto& dir : movement_directions(piece, false)) {
    const int next_x = x + dir.first;
    const int next_y = y + dir.second;
    if (!in_bounds(next_x, next_y) || board[next_y][next_x] != kEmpty) {
      continue;
    }
    Move move;
    move.from_x = x;
    move.from_y = y;
    move.path.push_back({next_x, next_y});
    if (piece == kWhiteMan && next_y == 0) {
      move.promotes = true;
    } else if (piece == kBlackMan && next_y == kBoardSize - 1) {
      move.promotes = true;
    }
    moves.push_back(move);
  }
  return moves;
}

std::vector<Move> generate_all_moves(const Board& board, bool white_turn) {
  std::vector<Move> captures;
  std::vector<Move> steps;
  for (int y = 0; y < kBoardSize; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
      const int piece = board[y][x];
      if (piece == kEmpty || is_white_piece(piece) != white_turn) {
        continue;
      }
      const auto piece_captures = generate_piece_moves(board, x, y, true);
      captures.insert(captures.end(), piece_captures.begin(), piece_captures.end());
    }
  }
  if (!captures.empty()) {
    return captures;
  }
  for (int y = 0; y < kBoardSize; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
      const int piece = board[y][x];
      if (piece == kEmpty || is_white_piece(piece) != white_turn) {
        continue;
      }
      const auto piece_steps = generate_piece_moves(board, x, y, false);
      steps.insert(steps.end(), piece_steps.begin(), piece_steps.end());
    }
  }
  return steps;
}

Board apply_move(const Board& board, const Move& move) {
  Board next = board;
  int piece = next[move.from_y][move.from_x];
  next[move.from_y][move.from_x] = kEmpty;
  int current_x = move.from_x;
  int current_y = move.from_y;
  for (std::size_t index = 0; index < move.path.size(); ++index) {
    const auto& step = move.path[index];
    if (index < move.captures.size()) {
      next[move.captures[index].second][move.captures[index].first] = kEmpty;
    }
    current_x = step.first;
    current_y = step.second;
  }
  if (piece == kWhiteMan && current_y == 0) {
    piece = kWhiteKing;
  } else if (piece == kBlackMan && current_y == kBoardSize - 1) {
    piece = kBlackKing;
  }
  next[current_y][current_x] = piece;
  return next;
}

int evaluate_board(const Board& board) {
  int score = 0;
  for (int y = 0; y < kBoardSize; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
      const int piece = board[y][x];
      switch (piece) {
        case kWhiteMan:
          score -= 100 + (7 - y) * 5;
          if (x >= 2 && x <= 5 && y >= 2 && y <= 5) {
            score -= 6;
          }
          break;
        case kWhiteKing:
          score -= 180;
          break;
        case kBlackMan:
          score += 100 + y * 5;
          if (x >= 2 && x <= 5 && y >= 2 && y <= 5) {
            score += 6;
          }
          break;
        case kBlackKing:
          score += 180;
          break;
        default:
          break;
      }
    }
  }
  score += static_cast<int>(generate_all_moves(board, false).size()) * 2;
  score -= static_cast<int>(generate_all_moves(board, true).size()) * 2;
  return score;
}

void order_moves(std::vector<Move>& moves) {
  std::sort(moves.begin(), moves.end(), [](const Move& left, const Move& right) {
    if (left.captures.size() != right.captures.size()) {
      return left.captures.size() > right.captures.size();
    }
    if (left.promotes != right.promotes) {
      return left.promotes;
    }
    return left.path.size() > right.path.size();
  });
}

int search(const Board& board, bool white_turn, int depth, int alpha, int beta) {
  auto moves = generate_all_moves(board, white_turn);
  if (depth == 0 || moves.empty()) {
    if (moves.empty()) {
      return white_turn ? 200000 : -200000;
    }
    return evaluate_board(board);
  }
  order_moves(moves);
  if (white_turn) {
    int best = 300000;
    for (const auto& move : moves) {
      const int value = search(apply_move(board, move), false, depth - 1, alpha, beta);
      best = std::min(best, value);
      beta = std::min(beta, best);
      if (beta <= alpha) {
        break;
      }
    }
    return best;
  }
  int best = -300000;
  for (const auto& move : moves) {
    const int value = search(apply_move(board, move), true, depth - 1, alpha, beta);
    best = std::max(best, value);
    alpha = std::max(alpha, best);
    if (beta <= alpha) {
      break;
    }
  }
  return best;
}

Move choose_ai_move(const Board& board) {
  auto moves = generate_all_moves(board, false);
  order_moves(moves);
  Move best_move = moves.front();
  int best_score = -300000;
  for (const auto& move : moves) {
    const int value = search(apply_move(board, move), true, kSearchDepth - 1, -300000, 300000);
    if (value > best_score) {
      best_score = value;
      best_move = move;
    }
  }
  return best_move;
}

std::vector<Move> moves_for_piece(const std::vector<Move>& legal_moves, int x, int y) {
  std::vector<Move> filtered;
  for (const auto& move : legal_moves) {
    if (move.from_x == x && move.from_y == y) {
      filtered.push_back(move);
    }
  }
  return filtered;
}

void refresh_legal_moves(GameState& game) {
  game.legal_moves = generate_all_moves(game.board, game.white_turn);
  if (game.legal_moves.empty()) {
    game.game_over = true;
    game.winner = game.white_turn ? -1 : 1;
  }
}

void reset_game(GameState& game) {
  setup_board(game.board);
  game.white_turn = true;
  game.paused = false;
  game.game_over = false;
  game.winner = 0;
  game.cursor_x = 1;
  game.cursor_y = 5;
  game.selected_x = -1;
  game.selected_y = -1;
  game.ai_ready_at = 0U;
  refresh_legal_moves(game);
}

void start_match(GameState& game, bool versus_cpu) {
  game.versus_cpu = versus_cpu;
  reset_game(game);
  game.mode_select_active = false;
}

void draw_piece(SDL_Renderer* renderer, const Theme& theme, int piece, int x, int y, bool selected) {
  const int center_x = kBoardX + x * kCellSize + kCellSize / 2;
  const int center_y = kBoardY + y * kCellSize + kCellSize / 2;
  const SDL_Color base = is_white_piece(piece) ? theme.white_piece : theme.black_piece;
  fill_circle(renderer, center_x, center_y, 14, base);
  fill_circle(renderer, center_x, center_y, 11, is_white_piece(piece) ? theme.light_square : theme.panel_highlight);
  if (is_king_piece(piece)) {
    fill_circle(renderer, center_x, center_y, 8, theme.king_ring);
    fill_circle(renderer, center_x, center_y, 4, base);
  }
  if (selected) {
    draw_rect(renderer, {center_x - 18, center_y - 18, 36, 36}, theme.cursor);
  }
}

void draw_board(SDL_Renderer* renderer, const Theme& theme, const GameState& game) {
  fill_rect(renderer, {kBoardX - 8, kBoardY - 8, kCellSize * kBoardSize + 16, kCellSize * kBoardSize + 16},
            theme.frame);
  fill_rect(renderer, {kBoardX - 4, kBoardY - 4, kCellSize * kBoardSize + 8, kCellSize * kBoardSize + 8},
            theme.panel);

  const auto selected_moves = game.selected_x >= 0 ? moves_for_piece(game.legal_moves, game.selected_x, game.selected_y)
                                                   : std::vector<Move>{};

  for (int y = 0; y < kBoardSize; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
      SDL_Rect square{kBoardX + x * kCellSize, kBoardY + y * kCellSize, kCellSize, kCellSize};
      fill_rect(renderer, square, is_dark_square(x, y) ? theme.dark_square : theme.light_square);
      if (x == game.cursor_x && y == game.cursor_y) {
        draw_rect(renderer, {square.x + 1, square.y + 1, square.w - 2, square.h - 2}, theme.cursor);
      }
      if (game.board[y][x] != kEmpty) {
        draw_piece(renderer, theme, game.board[y][x], x, y, x == game.selected_x && y == game.selected_y);
      }
    }
  }

  for (const auto& move : selected_moves) {
    const auto& final_step = move.path.back();
    fill_circle(renderer,
                kBoardX + final_step.first * kCellSize + kCellSize / 2,
                kBoardY + final_step.second * kCellSize + kCellSize / 2,
                6, theme.move_hint);
  }
}

void draw_sidebar(SDL_Renderer* renderer, const Theme& theme, const GameState& game) {
  int white_count = 0;
  int black_count = 0;
  const int left_x = kPanelX + 12;
  const int right_x = kPanelX + kPanelW - 12;
  for (const auto& row : game.board) {
    for (int piece : row) {
      if (is_white_piece(piece)) {
        ++white_count;
      } else if (is_black_piece(piece)) {
        ++black_count;
      }
    }
  }

  fill_rect(renderer, {kPanelX + 6, kPanelY + 8, kPanelW, kPanelH}, theme.frame);
  fill_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, theme.panel);
  draw_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, theme.muted);

  draw_text(renderer, "TURN", left_x, kPanelY + 16, 1, theme.muted, false);
  draw_text_right(renderer, game.white_turn ? "WHITE" : "BLACK", right_x, kPanelY + 30, 2, theme.text);

  draw_text(renderer, "WHITE", left_x, kPanelY + 64, 1, theme.muted, false);
  draw_text_right(renderer, std::to_string(white_count), right_x, kPanelY + 78, 2, theme.text);
  draw_text(renderer, "BLACK", left_x, kPanelY + 106, 1, theme.muted, false);
  draw_text_right(renderer, std::to_string(black_count), right_x, kPanelY + 120, 2, theme.text);

  fill_rect(renderer, {kPanelX + 10, kPanelY + 150, kPanelW - 20, 94}, theme.panel_highlight);
  draw_text(renderer, "A SELECT", kPanelX + 18, kPanelY + 162, 1, theme.text, false);
  draw_text(renderer, "B CANCEL", kPanelX + 18, kPanelY + 180, 1, theme.text, false);
  draw_text(renderer, "START PAUSE", kPanelX + 18, kPanelY + 198, 1, theme.text, false);
  draw_text(renderer, "SELECT EXIT", kPanelX + 18, kPanelY + 216, 1, theme.text, false);

  if (!game.game_over) {
    if (!game.versus_cpu) {
      draw_text(renderer, "LOCAL MATCH", kPanelX + kPanelW / 2, kPanelY + 268, 1, theme.muted, true);
      draw_text(renderer, "PASS DEVICE", kPanelX + kPanelW / 2, kPanelY + 284, 2, theme.text, true);
    } else if (game.white_turn) {
      draw_text(renderer, "CAPTURES ARE", kPanelX + kPanelW / 2, kPanelY + 268, 1, theme.muted, true);
      draw_text(renderer, "MANDATORY", kPanelX + kPanelW / 2, kPanelY + 284, 2, theme.text, true);
    } else {
      draw_text(renderer, "CPU", kPanelX + kPanelW / 2, kPanelY + 268, 2, theme.muted, true);
      draw_text(renderer, "ANALYZING", kPanelX + kPanelW / 2, kPanelY + 292, 2, theme.text, true);
    }
  }
}

void draw_overlay(SDL_Renderer* renderer, const Theme& theme, const std::string& title, const std::string& subtitle) {
  fill_rect(renderer, {82, 206, 348, 84}, theme.overlay);
  draw_rect(renderer, {82, 206, 348, 84}, theme.frame);
  draw_text(renderer, title, kWindowWidth / 2, 224, 4, theme.text, true);
  draw_text(renderer, subtitle, kWindowWidth / 2, 258, 2, theme.muted, true);
}

void draw_mode_overlay(SDL_Renderer* renderer, const Theme& theme, int mode_index) {
  const SDL_Rect box{94, 188, 324, 92};
  const SDL_Rect cpu_card{118, 224, 116, 42};
  const SDL_Rect local_card{278, 224, 116, 42};
  fill_rect(renderer, box, theme.overlay);
  draw_rect(renderer, box, theme.frame);
  draw_text(renderer, "CHOOSE MODE", kWindowWidth / 2, 198, 3, theme.text, true);

  fill_rect(renderer, cpu_card, mode_index == 0 ? theme.cursor : theme.panel_highlight);
  draw_rect(renderer, cpu_card, theme.frame);
  draw_text(renderer, "CPU", cpu_card.x + cpu_card.w / 2, cpu_card.y + 8, 2,
            mode_index == 0 ? theme.black_piece : theme.text, true);
  draw_text(renderer, "OPPONENT", cpu_card.x + cpu_card.w / 2, cpu_card.y + 24, 1,
            mode_index == 0 ? theme.black_piece : theme.text, true);

  fill_rect(renderer, local_card, mode_index == 1 ? theme.cursor : theme.panel_highlight);
  draw_rect(renderer, local_card, theme.frame);
  draw_text(renderer, "2 PLAYER", local_card.x + local_card.w / 2, local_card.y + 8, 1,
            mode_index == 1 ? theme.black_piece : theme.text, true);
  draw_text(renderer, "SAME DEVICE", local_card.x + local_card.w / 2, local_card.y + 24, 1,
            mode_index == 1 ? theme.black_piece : theme.text, true);
}

bool move_matches_destination(const Move& move, int x, int y) {
  if (move.path.empty()) {
    return false;
  }
  return move.path.back().first == x && move.path.back().second == y;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  pp_context context{};
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_AudioDeviceID audio_device = 0U;
  pp_audio_spec audio_spec{};
  ToneState tone;
  Theme theme;
  GameState game;
  pp_input_state input{};
  pp_input_state previous{};
  int width = 0;
  int height = 0;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (pp_init(&context, "pixelpal-checkers") != 0) {
    std::fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }
  pp_get_framebuffer_size(&context, &width, &height);
  width = std::max(width, 512);
  height = std::max(height, 512);

  window = SDL_CreateWindow("Checkers", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderSetLogicalSize(renderer, kWindowWidth, kWindowHeight);

  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &tone;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  reset_game(game);
  game.mode_select_active = true;

  while (!pp_should_exit(&context)) {
    const Uint32 now = SDL_GetTicks();
    pp_poll_input(&context, &input);

    const bool pressed_up = input.up && !previous.up;
    const bool pressed_down = input.down && !previous.down;
    const bool pressed_left = input.left && !previous.left;
    const bool pressed_right = input.right && !previous.right;
    const bool pressed_a = input.a && !previous.a;
    const bool pressed_b = input.b && !previous.b;
    const bool pressed_start = input.start && !previous.start;

    if (game.mode_select_active) {
      if (pressed_left || pressed_up) {
        game.mode_index = 0;
        trigger_tone(tone, 420.0f, 18);
      }
      if (pressed_right || pressed_down) {
        game.mode_index = 1;
        trigger_tone(tone, 420.0f, 18);
      }
      if (pressed_a || pressed_start) {
        start_match(game, game.mode_index == 0);
        trigger_tone(tone, 780.0f, 80);
      }
    } else if (pressed_start) {
      if (game.game_over) {
        reset_game(game);
        trigger_tone(tone, 780.0f, 80);
      } else {
        game.paused = !game.paused;
        trigger_tone(tone, game.paused ? 240.0f : 640.0f, 48);
      }
    }

    if (!game.mode_select_active && !game.paused && !game.game_over) {
      if (game.white_turn || !game.versus_cpu) {
        if (pressed_left) {
          game.cursor_x = std::max(0, game.cursor_x - 1);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_right) {
          game.cursor_x = std::min(kBoardSize - 1, game.cursor_x + 1);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_up) {
          game.cursor_y = std::max(0, game.cursor_y - 1);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_down) {
          game.cursor_y = std::min(kBoardSize - 1, game.cursor_y + 1);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_b) {
          game.selected_x = -1;
          game.selected_y = -1;
          trigger_tone(tone, 320.0f, 26);
        }
        if (pressed_a) {
          bool moved = false;
          if (game.selected_x >= 0) {
            const auto selected_moves = moves_for_piece(game.legal_moves, game.selected_x, game.selected_y);
            for (const auto& move : selected_moves) {
              if (move_matches_destination(move, game.cursor_x, game.cursor_y)) {
                game.board = apply_move(game.board, move);
                game.white_turn = !game.white_turn;
                game.selected_x = -1;
                game.selected_y = -1;
                refresh_legal_moves(game);
                if (game.versus_cpu && !game.white_turn && !game.game_over) {
                  game.ai_ready_at = now + kAiDelayMs;
                } else {
                  game.ai_ready_at = 0U;
                }
                moved = true;
                trigger_tone(tone, move.captures.empty() ? 720.0f : 860.0f, 40);
                break;
              }
            }
          }
          if (!moved) {
            const auto selectable = moves_for_piece(game.legal_moves, game.cursor_x, game.cursor_y);
            const int piece = game.board[game.cursor_y][game.cursor_x];
            const bool current_player_piece = game.white_turn ? is_white_piece(piece) : is_black_piece(piece);
            if (!selectable.empty() && current_player_piece) {
              game.selected_x = game.cursor_x;
              game.selected_y = game.cursor_y;
              trigger_tone(tone, 580.0f, 28);
            }
          }
        }
      } else if (game.versus_cpu && now >= game.ai_ready_at) {
        if (!game.legal_moves.empty()) {
          const Move best = choose_ai_move(game.board);
          game.board = apply_move(game.board, best);
          game.white_turn = true;
          refresh_legal_moves(game);
          trigger_tone(tone, best.captures.empty() ? 360.0f : 200.0f, 48);
        }
      }
    }

    fill_rect(renderer, {0, 0, kWindowWidth, kWindowHeight}, theme.background);
    draw_text(renderer, "CHECKERS", kWindowWidth / 2, 34, 5, theme.text, true);
    draw_text(renderer,
              game.versus_cpu ? "CLASSIC RULES / FORCED CAPTURES / CPU OPPONENT"
                              : "CLASSIC RULES / FORCED CAPTURES / 2 PLAYER",
              kWindowWidth / 2, 72, 1, theme.muted, true);
    draw_board(renderer, theme, game);
    draw_sidebar(renderer, theme, game);

    if (game.mode_select_active) {
      draw_mode_overlay(renderer, theme, game.mode_index);
    } else if (game.paused && !game.game_over) {
      draw_overlay(renderer, theme, "PAUSED", "START TO RESUME");
    } else if (game.game_over) {
      draw_overlay(renderer, theme,
                   game.winner > 0 ? "WHITE WINS" : "BLACK WINS",
                   "START FOR NEW MATCH");
    }

    SDL_RenderPresent(renderer);
    previous = input;
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
