#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kWindowWidth = 512;
constexpr int kWindowHeight = 512;
constexpr int kBoardSize = 8;
constexpr int kCellSize = 40;
constexpr int kBoardX = 20;
constexpr int kBoardY = 96;
constexpr int kPanelX = 348;
constexpr int kPanelY = 96;
constexpr int kPanelW = 144;
constexpr int kPanelH = 320;
constexpr int kAiDelayMs = 260;
constexpr int kSearchDepth = 4;
constexpr int kMateScore = 100000;

enum Piece {
  kNone = 0,
  kPawn = 1,
  kKnight = 2,
  kBishop = 3,
  kRook = 4,
  kQueen = 5,
  kKing = 6,
};

using Board = std::array<int, 64>;

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Move {
  int from = 0;
  int to = 0;
  int promotion = 0;
  bool en_passant = false;
  bool castle_kingside = false;
  bool castle_queenside = false;
};

struct Theme {
  SDL_Color background{17, 20, 28, 255};
  SDL_Color frame{74, 87, 117, 255};
  SDL_Color panel{31, 37, 52, 255};
  SDL_Color panel_highlight{46, 56, 79, 255};
  SDL_Color light_square{213, 219, 227, 255};
  SDL_Color dark_square{96, 117, 152, 255};
  SDL_Color cursor{246, 203, 112, 255};
  SDL_Color move_hint{115, 225, 214, 255};
  SDL_Color text{241, 243, 247, 255};
  SDL_Color muted{167, 176, 194, 255};
  SDL_Color white_piece{245, 246, 248, 255};
  SDL_Color black_piece{43, 48, 60, 255};
  SDL_Color white_outline{98, 104, 120, 255};
  SDL_Color black_outline{12, 14, 18, 255};
  SDL_Color accent{255, 184, 98, 255};
  SDL_Color overlay{8, 11, 18, 222};
};

struct GameState {
  Board board{};
  bool white_turn = true;
  bool white_king_castle = true;
  bool white_queen_castle = true;
  bool black_king_castle = true;
  bool black_queen_castle = true;
  int en_passant = -1;
  int halfmove_clock = 0;
  int ply_count = 0;
  bool paused = false;
  bool game_over = false;
  bool draw = false;
  int winner = 0;
  int cursor = 52;
  int selected = -1;
  Uint32 ai_ready_at = 0U;
  std::vector<Move> legal_moves;
  bool promotion_pending = false;
  std::vector<Move> promotion_moves;
  int promotion_index = 0;
  std::string end_message;
  bool versus_cpu = true;
  bool mode_select_active = true;
  int mode_index = 0;
};

constexpr std::array<int, 64> kPawnTable = {
    0, 0, 0, 0, 0, 0, 0, 0,
    5, 10, 10, -20, -20, 10, 10, 5,
    5, -5, -10, 0, 0, -10, -5, 5,
    0, 0, 0, 20, 20, 0, 0, 0,
    5, 5, 10, 25, 25, 10, 5, 5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
    0, 0, 0, 0, 0, 0, 0, 0};

constexpr std::array<int, 64> kKnightTable = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20, 0, 5, 5, 0, -20, -40,
    -30, 5, 10, 15, 15, 10, 5, -30,
    -30, 0, 15, 20, 20, 15, 0, -30,
    -30, 5, 15, 20, 20, 15, 5, -30,
    -30, 0, 10, 15, 15, 10, 0, -30,
    -40, -20, 0, 0, 0, 0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50};

constexpr std::array<int, 64> kBishopTable = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10, 5, 0, 0, 0, 0, 5, -10,
    -10, 10, 10, 10, 10, 10, 10, -10,
    -10, 0, 10, 10, 10, 10, 0, -10,
    -10, 5, 5, 10, 10, 5, 5, -10,
    -10, 0, 5, 10, 10, 5, 0, -10,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

constexpr std::array<int, 64> kRookTable = {
    0, 0, 0, 5, 5, 0, 0, 0,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    5, 10, 10, 10, 10, 10, 10, 5,
    0, 0, 0, 0, 0, 0, 0, 0};

constexpr std::array<int, 64> kQueenTable = {
    -20, -10, -10, -5, -5, -10, -10, -20,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -10, 0, 5, 5, 5, 5, 0, -10,
    -5, 0, 5, 5, 5, 5, 0, -5,
    0, 0, 5, 5, 5, 5, 0, -5,
    -10, 5, 5, 5, 5, 5, 0, -10,
    -10, 0, 5, 0, 0, 0, 0, -10,
    -20, -10, -10, -5, -5, -10, -10, -20};

constexpr std::array<int, 64> kKingTable = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    20, 20, 0, 0, 0, 0, 20, 20,
    20, 30, 10, 0, 0, 10, 30, 20};

bool in_bounds(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }
int make_sq(int file, int rank) { return rank * 8 + file; }
int file_of(int sq) { return sq % 8; }
int rank_of(int sq) { return sq / 8; }
bool is_white_piece(int piece) { return piece > 0; }
int piece_type(int piece) { return std::abs(piece); }
int sign(int value) { return value < 0 ? -1 : (value > 0 ? 1 : 0); }

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
  board = {
      -kRook, -kKnight, -kBishop, -kQueen, -kKing, -kBishop, -kKnight, -kRook,
      -kPawn, -kPawn,   -kPawn,   -kPawn,  -kPawn, -kPawn,   -kPawn,   -kPawn,
      0,      0,        0,        0,       0,      0,        0,        0,
      0,      0,        0,        0,       0,      0,        0,        0,
      0,      0,        0,        0,       0,      0,        0,        0,
      0,      0,        0,        0,       0,      0,        0,        0,
      kPawn,  kPawn,    kPawn,    kPawn,   kPawn,  kPawn,    kPawn,    kPawn,
      kRook,  kKnight,  kBishop,  kQueen,  kKing,  kBishop,  kKnight,  kRook};
}

int find_king(const Board& board, bool white) {
  const int target = white ? kKing : -kKing;
  for (int sq = 0; sq < 64; ++sq) {
    if (board[sq] == target) {
      return sq;
    }
  }
  return -1;
}

bool is_square_attacked(const GameState& state, int sq, bool by_white) {
  const int file = file_of(sq);
  const int rank = rank_of(sq);

  const int pawn_rank = rank + (by_white ? 1 : -1);
  const int pawn_piece = by_white ? kPawn : -kPawn;
  for (int df : {-1, 1}) {
    const int source_file = file + df;
    if (in_bounds(source_file, pawn_rank) &&
        state.board[make_sq(source_file, pawn_rank)] == pawn_piece) {
      return true;
    }
  }

  const int knight_piece = by_white ? kKnight : -kKnight;
  constexpr std::array<std::pair<int, int>, 8> kKnightSteps{{
      {1, 2}, {2, 1}, {-1, 2}, {-2, 1},
      {1, -2}, {2, -1}, {-1, -2}, {-2, -1},
  }};
  for (const auto& step : kKnightSteps) {
    const int next_file = file + step.first;
    const int next_rank = rank + step.second;
    if (in_bounds(next_file, next_rank) &&
        state.board[make_sq(next_file, next_rank)] == knight_piece) {
      return true;
    }
  }

  const int bishop_piece = by_white ? kBishop : -kBishop;
  const int rook_piece = by_white ? kRook : -kRook;
  const int queen_piece = by_white ? kQueen : -kQueen;
  const int king_piece = by_white ? kKing : -kKing;

  constexpr std::array<std::pair<int, int>, 4> kDiagDirs{{
      {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
  }};
  for (const auto& dir : kDiagDirs) {
    int next_file = file + dir.first;
    int next_rank = rank + dir.second;
    while (in_bounds(next_file, next_rank)) {
      const int piece = state.board[make_sq(next_file, next_rank)];
      if (piece != 0) {
        if (piece == bishop_piece || piece == queen_piece) {
          return true;
        }
        break;
      }
      next_file += dir.first;
      next_rank += dir.second;
    }
  }

  constexpr std::array<std::pair<int, int>, 4> kLineDirs{{
      {1, 0}, {-1, 0}, {0, 1}, {0, -1},
  }};
  for (const auto& dir : kLineDirs) {
    int next_file = file + dir.first;
    int next_rank = rank + dir.second;
    while (in_bounds(next_file, next_rank)) {
      const int piece = state.board[make_sq(next_file, next_rank)];
      if (piece != 0) {
        if (piece == rook_piece || piece == queen_piece) {
          return true;
        }
        break;
      }
      next_file += dir.first;
      next_rank += dir.second;
    }
  }

  constexpr std::array<std::pair<int, int>, 8> kKingSteps{{
      {1, 0}, {-1, 0}, {0, 1}, {0, -1},
      {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
  }};
  for (const auto& step : kKingSteps) {
    const int next_file = file + step.first;
    const int next_rank = rank + step.second;
    if (in_bounds(next_file, next_rank) &&
        state.board[make_sq(next_file, next_rank)] == king_piece) {
      return true;
    }
  }

  return false;
}

GameState apply_move(const GameState& state, const Move& move) {
  GameState next{};
  next.board = state.board;
  next.white_turn = state.white_turn;
  next.white_king_castle = state.white_king_castle;
  next.white_queen_castle = state.white_queen_castle;
  next.black_king_castle = state.black_king_castle;
  next.black_queen_castle = state.black_queen_castle;
  next.en_passant = state.en_passant;
  next.halfmove_clock = state.halfmove_clock;
  next.ply_count = state.ply_count;
  next.cursor = state.cursor;
  next.versus_cpu = state.versus_cpu;
  next.mode_select_active = state.mode_select_active;
  next.mode_index = state.mode_index;

  const int moving_piece = next.board[move.from];
  const int captured_piece = next.board[move.to];
  next.board[move.from] = 0;
  next.board[move.to] = moving_piece;
  next.en_passant = -1;

  if (move.en_passant) {
    const int captured_rank = rank_of(move.to) + (moving_piece > 0 ? 1 : -1);
    next.board[make_sq(file_of(move.to), captured_rank)] = 0;
  }
  if (piece_type(moving_piece) == kPawn && std::abs(rank_of(move.to) - rank_of(move.from)) == 2) {
    next.en_passant = make_sq(file_of(move.from), (rank_of(move.to) + rank_of(move.from)) / 2);
  }
  if (move.promotion != 0) {
    next.board[move.to] = sign(moving_piece) * move.promotion;
  }
  if (move.castle_kingside) {
    if (moving_piece > 0) {
      next.board[63] = 0;
      next.board[61] = kRook;
    } else {
      next.board[7] = 0;
      next.board[5] = -kRook;
    }
  } else if (move.castle_queenside) {
    if (moving_piece > 0) {
      next.board[56] = 0;
      next.board[59] = kRook;
    } else {
      next.board[0] = 0;
      next.board[3] = -kRook;
    }
  }

  if (moving_piece == kKing) {
    next.white_king_castle = false;
    next.white_queen_castle = false;
  } else if (moving_piece == -kKing) {
    next.black_king_castle = false;
    next.black_queen_castle = false;
  } else if (moving_piece == kRook) {
    if (move.from == 63) next.white_king_castle = false;
    if (move.from == 56) next.white_queen_castle = false;
  } else if (moving_piece == -kRook) {
    if (move.from == 7) next.black_king_castle = false;
    if (move.from == 0) next.black_queen_castle = false;
  }
  if (captured_piece == kRook) {
    if (move.to == 63) next.white_king_castle = false;
    if (move.to == 56) next.white_queen_castle = false;
  } else if (captured_piece == -kRook) {
    if (move.to == 7) next.black_king_castle = false;
    if (move.to == 0) next.black_queen_castle = false;
  }

  if (piece_type(moving_piece) == kPawn || captured_piece != 0 || move.en_passant) {
    next.halfmove_clock = 0;
  } else {
    ++next.halfmove_clock;
  }
  ++next.ply_count;

  next.white_turn = !state.white_turn;
  next.selected = -1;
  next.promotion_pending = false;
  next.promotion_index = 0;
  next.legal_moves.clear();
  next.promotion_moves.clear();
  next.end_message.clear();
  return next;
}

std::vector<Move> generate_pseudo_moves(const GameState& state) {
  std::vector<Move> moves;
  for (int sq = 0; sq < 64; ++sq) {
    const int piece = state.board[sq];
    if (piece == 0 || is_white_piece(piece) != state.white_turn) {
      continue;
    }

    const int file = file_of(sq);
    const int rank = rank_of(sq);
    const int type = piece_type(piece);

    if (type == kPawn) {
      const int dir = state.white_turn ? -1 : 1;
      const int start_rank = state.white_turn ? 6 : 1;
      const int promotion_rank = state.white_turn ? 0 : 7;
      const int one_rank = rank + dir;

      if (in_bounds(file, one_rank) && state.board[make_sq(file, one_rank)] == 0) {
        Move move{sq, make_sq(file, one_rank)};
        if (one_rank == promotion_rank) {
          for (int promo : {kQueen, kRook, kBishop, kKnight}) {
            Move promoted = move;
            promoted.promotion = promo;
            moves.push_back(promoted);
          }
        } else {
          moves.push_back(move);
          const int two_rank = rank + dir * 2;
          if (rank == start_rank && state.board[make_sq(file, two_rank)] == 0) {
            moves.push_back({sq, make_sq(file, two_rank)});
          }
        }
      }

      for (int df : {-1, 1}) {
        const int next_file = file + df;
        const int next_rank = rank + dir;
        if (!in_bounds(next_file, next_rank)) {
          continue;
        }
        const int target_sq = make_sq(next_file, next_rank);
        const int target = state.board[target_sq];
        if (target != 0 && is_white_piece(target) != state.white_turn) {
          Move move{sq, target_sq};
          if (next_rank == promotion_rank) {
            for (int promo : {kQueen, kRook, kBishop, kKnight}) {
              Move promoted = move;
              promoted.promotion = promo;
              moves.push_back(promoted);
            }
          } else {
            moves.push_back(move);
          }
        } else if (state.en_passant == target_sq) {
          Move move{sq, target_sq};
          move.en_passant = true;
          moves.push_back(move);
        }
      }
      continue;
    }

    if (type == kKnight) {
      constexpr std::array<std::pair<int, int>, 8> kKnightSteps{{
          {1, 2}, {2, 1}, {-1, 2}, {-2, 1},
          {1, -2}, {2, -1}, {-1, -2}, {-2, -1},
      }};
      for (const auto& step : kKnightSteps) {
        const int next_file = file + step.first;
        const int next_rank = rank + step.second;
        if (!in_bounds(next_file, next_rank)) {
          continue;
        }
        const int target_sq = make_sq(next_file, next_rank);
        const int target = state.board[target_sq];
        if (target == 0 || is_white_piece(target) != state.white_turn) {
          moves.push_back({sq, target_sq});
        }
      }
      continue;
    }

    if (type == kBishop || type == kRook || type == kQueen) {
      std::vector<std::pair<int, int>> dirs;
      if (type == kBishop || type == kQueen) {
        dirs.push_back({1, 1});
        dirs.push_back({-1, 1});
        dirs.push_back({1, -1});
        dirs.push_back({-1, -1});
      }
      if (type == kRook || type == kQueen) {
        dirs.push_back({1, 0});
        dirs.push_back({-1, 0});
        dirs.push_back({0, 1});
        dirs.push_back({0, -1});
      }
      for (const auto& dir : dirs) {
        int next_file = file + dir.first;
        int next_rank = rank + dir.second;
        while (in_bounds(next_file, next_rank)) {
          const int target_sq = make_sq(next_file, next_rank);
          const int target = state.board[target_sq];
          if (target == 0) {
            moves.push_back({sq, target_sq});
          } else {
            if (is_white_piece(target) != state.white_turn) {
              moves.push_back({sq, target_sq});
            }
            break;
          }
          next_file += dir.first;
          next_rank += dir.second;
        }
      }
      continue;
    }

    constexpr std::array<std::pair<int, int>, 8> kKingSteps{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
    }};
    for (const auto& step : kKingSteps) {
      const int next_file = file + step.first;
      const int next_rank = rank + step.second;
      if (!in_bounds(next_file, next_rank)) {
        continue;
      }
      const int target_sq = make_sq(next_file, next_rank);
      const int target = state.board[target_sq];
      if (target == 0 || is_white_piece(target) != state.white_turn) {
        moves.push_back({sq, target_sq});
      }
    }

    if (state.white_turn && sq == 60) {
      if (state.white_king_castle && state.board[61] == 0 && state.board[62] == 0 &&
          !is_square_attacked(state, 60, false) && !is_square_attacked(state, 61, false) &&
          !is_square_attacked(state, 62, false) && state.board[63] == kRook) {
        Move move{60, 62};
        move.castle_kingside = true;
        moves.push_back(move);
      }
      if (state.white_queen_castle && state.board[59] == 0 && state.board[58] == 0 && state.board[57] == 0 &&
          !is_square_attacked(state, 60, false) && !is_square_attacked(state, 59, false) &&
          !is_square_attacked(state, 58, false) && state.board[56] == kRook) {
        Move move{60, 58};
        move.castle_queenside = true;
        moves.push_back(move);
      }
    } else if (!state.white_turn && sq == 4) {
      if (state.black_king_castle && state.board[5] == 0 && state.board[6] == 0 &&
          !is_square_attacked(state, 4, true) && !is_square_attacked(state, 5, true) &&
          !is_square_attacked(state, 6, true) && state.board[7] == -kRook) {
        Move move{4, 6};
        move.castle_kingside = true;
        moves.push_back(move);
      }
      if (state.black_queen_castle && state.board[3] == 0 && state.board[2] == 0 && state.board[1] == 0 &&
          !is_square_attacked(state, 4, true) && !is_square_attacked(state, 3, true) &&
          !is_square_attacked(state, 2, true) && state.board[0] == -kRook) {
        Move move{4, 2};
        move.castle_queenside = true;
        moves.push_back(move);
      }
    }
  }
  return moves;
}

bool move_is_legal(const GameState& state, const Move& move) {
  const bool mover_is_white = state.white_turn;
  const GameState next = apply_move(state, move);
  const int king_sq = find_king(next.board, mover_is_white);
  return king_sq >= 0 && !is_square_attacked(next, king_sq, !mover_is_white);
}

std::vector<Move> legal_moves(const GameState& state) {
  std::vector<Move> moves;
  for (const auto& move : generate_pseudo_moves(state)) {
    if (move_is_legal(state, move)) {
      moves.push_back(move);
    }
  }
  return moves;
}

int material_value(int type) {
  switch (type) {
    case kPawn: return 100;
    case kKnight: return 320;
    case kBishop: return 330;
    case kRook: return 500;
    case kQueen: return 900;
    case kKing: return 20000;
    default: return 0;
  }
}

int mirror_sq(int sq) {
  return make_sq(file_of(sq), 7 - rank_of(sq));
}

int piece_square_value(int piece, int sq) {
  const int type = piece_type(piece);
  const int index = piece > 0 ? sq : mirror_sq(sq);
  const int raw = [&]() {
    switch (type) {
      case kPawn: return kPawnTable[index];
      case kKnight: return kKnightTable[index];
      case kBishop: return kBishopTable[index];
      case kRook: return kRookTable[index];
      case kQueen: return kQueenTable[index];
      case kKing: return kKingTable[index];
      default: return 0;
    }
  }();
  return piece > 0 ? raw : -raw;
}

bool insufficient_material(const Board& board) {
  int white_bishops = 0;
  int black_bishops = 0;
  int white_knights = 0;
  int black_knights = 0;
  int white_bishop_color = -1;
  int black_bishop_color = -1;

  for (int sq = 0; sq < 64; ++sq) {
    const int piece = board[sq];
    if (piece == 0) {
      continue;
    }
    const int type = piece_type(piece);
    if (type == kPawn || type == kRook || type == kQueen) {
      return false;
    }
    if (type == kBishop) {
      if (piece > 0) {
        ++white_bishops;
        white_bishop_color = (file_of(sq) + rank_of(sq)) % 2;
      } else {
        ++black_bishops;
        black_bishop_color = (file_of(sq) + rank_of(sq)) % 2;
      }
    } else if (type == kKnight) {
      if (piece > 0) {
        ++white_knights;
      } else {
        ++black_knights;
      }
    }
  }

  const int white_minors = white_bishops + white_knights;
  const int black_minors = black_bishops + black_knights;
  if (white_minors == 0 && black_minors == 0) {
    return true;
  }
  if ((white_minors == 1 && black_minors == 0) || (white_minors == 0 && black_minors == 1)) {
    return true;
  }
  if (white_knights == 2 && white_bishops == 0 && black_minors == 0) {
    return true;
  }
  if (black_knights == 2 && black_bishops == 0 && white_minors == 0) {
    return true;
  }
  if (white_knights == 0 && black_knights == 0 && white_bishops <= 1 && black_bishops <= 1 &&
      white_bishops + black_bishops > 0) {
    if (white_bishops == 0 || black_bishops == 0 || white_bishop_color == black_bishop_color) {
      return true;
    }
  }
  return false;
}

int evaluate_position(const GameState& state) {
  int score = 0;
  int white_bishops = 0;
  int black_bishops = 0;

  for (int sq = 0; sq < 64; ++sq) {
    const int piece = state.board[sq];
    if (piece == 0) {
      continue;
    }
    const int side = sign(piece);
    score += side * material_value(piece_type(piece));
    score += piece_square_value(piece, sq);

    const int file = file_of(sq);
    const int rank = rank_of(sq);
    if (file >= 2 && file <= 5 && rank >= 2 && rank <= 5) {
      score += side * 8;
    }
    if (piece_type(piece) == kBishop) {
      if (piece > 0) {
        ++white_bishops;
      } else {
        ++black_bishops;
      }
    }
  }

  if (white_bishops >= 2) {
    score += 24;
  }
  if (black_bishops >= 2) {
    score -= 24;
  }
  if (state.white_king_castle) score += 10;
  if (state.white_queen_castle) score += 8;
  if (state.black_king_castle) score -= 10;
  if (state.black_queen_castle) score -= 8;

  return score;
}

int move_order_score(const GameState& state, const Move& move) {
  const int moving_piece = state.board[move.from];
  int captured_piece = state.board[move.to];
  if (move.en_passant) {
    captured_piece = state.white_turn ? -kPawn : kPawn;
  }

  int score = 0;
  if (captured_piece != 0) {
    score += 10 * material_value(piece_type(captured_piece)) - material_value(piece_type(moving_piece));
  }
  if (move.promotion != 0) {
    score += 800 + material_value(move.promotion);
  }
  if (move.castle_kingside || move.castle_queenside) {
    score += 60;
  }

  const int file = file_of(move.to);
  const int rank = rank_of(move.to);
  if (file >= 2 && file <= 5 && rank >= 2 && rank <= 5) {
    score += 12;
  }
  return score;
}

void sort_moves(std::vector<Move>& moves, const GameState& state) {
  std::sort(moves.begin(), moves.end(), [&](const Move& lhs, const Move& rhs) {
    return move_order_score(state, lhs) > move_order_score(state, rhs);
  });
}

int search(const GameState& state, int depth, int alpha, int beta, int ply) {
  if (state.halfmove_clock >= 100 || insufficient_material(state.board)) {
    return 0;
  }

  std::vector<Move> moves = legal_moves(state);
  if (moves.empty()) {
    const int king_sq = find_king(state.board, state.white_turn);
    if (king_sq >= 0 && is_square_attacked(state, king_sq, !state.white_turn)) {
      return -kMateScore + ply;
    }
    return 0;
  }

  if (depth == 0) {
    const int eval = evaluate_position(state);
    return state.white_turn ? eval : -eval;
  }

  sort_moves(moves, state);
  for (const auto& move : moves) {
    const GameState child = apply_move(state, move);
    const int score = -search(child, depth - 1, -beta, -alpha, ply + 1);
    if (score >= beta) {
      return beta;
    }
    if (score > alpha) {
      alpha = score;
    }
  }
  return alpha;
}

Move choose_ai_move(const GameState& state) {
  std::vector<Move> moves = state.legal_moves.empty() ? legal_moves(state) : state.legal_moves;
  sort_moves(moves, state);

  Move best = moves.front();
  int best_score = std::numeric_limits<int>::min();
  for (const auto& move : moves) {
    const GameState child = apply_move(state, move);
    const int score = -search(child, kSearchDepth - 1, -kMateScore, kMateScore, 1);
    if (score > best_score) {
      best_score = score;
      best = move;
    }
  }
  return best;
}

std::vector<Move> moves_for_square(const std::vector<Move>& moves, int square) {
  std::vector<Move> filtered;
  for (const auto& move : moves) {
    if (move.from == square) {
      filtered.push_back(move);
    }
  }
  return filtered;
}

bool square_has_legal_move(const GameState& game, int square) {
  for (const auto& move : game.legal_moves) {
    if (move.from == square) {
      return true;
    }
  }
  return false;
}

void refresh_game_state(GameState& game) {
  game.legal_moves = legal_moves(game);
  game.game_over = false;
  game.draw = false;
  game.winner = 0;
  game.end_message.clear();

  if (game.halfmove_clock >= 100) {
    game.game_over = true;
    game.draw = true;
    game.end_message = "FIFTY MOVE DRAW";
    return;
  }
  if (insufficient_material(game.board)) {
    game.game_over = true;
    game.draw = true;
    game.end_message = "INSUFFICIENT";
    return;
  }
  if (!game.legal_moves.empty()) {
    return;
  }

  game.game_over = true;
  const int king_sq = find_king(game.board, game.white_turn);
  if (king_sq >= 0 && is_square_attacked(game, king_sq, !game.white_turn)) {
    game.draw = false;
    game.winner = game.white_turn ? -1 : 1;
    game.end_message = "CHECKMATE";
  } else {
    game.draw = true;
    game.end_message = "STALEMATE";
  }
}

void reset_game(GameState& game) {
  setup_board(game.board);
  game.white_turn = true;
  game.white_king_castle = true;
  game.white_queen_castle = true;
  game.black_king_castle = true;
  game.black_queen_castle = true;
  game.en_passant = -1;
  game.halfmove_clock = 0;
  game.ply_count = 0;
  game.paused = false;
  game.game_over = false;
  game.draw = false;
  game.winner = 0;
  game.cursor = 52;
  game.selected = -1;
  game.ai_ready_at = 0U;
  game.promotion_pending = false;
  game.promotion_index = 0;
  game.promotion_moves.clear();
  game.end_message.clear();
  refresh_game_state(game);
}

void start_match(GameState& game, bool versus_cpu) {
  game.versus_cpu = versus_cpu;
  reset_game(game);
  game.mode_select_active = false;
}

char piece_symbol(int piece) {
  switch (piece_type(piece)) {
    case kPawn: return 'P';
    case kKnight: return 'N';
    case kBishop: return 'B';
    case kRook: return 'R';
    case kQueen: return 'Q';
    case kKing: return 'K';
    default: return ' ';
  }
}

void draw_piece(SDL_Renderer* renderer, const Theme& theme, int piece, int sq, bool selected) {
  if (piece == 0) {
    return;
  }

  const int file = file_of(sq);
  const int rank = rank_of(sq);
  const int x = kBoardX + file * kCellSize;
  const int y = kBoardY + rank * kCellSize;
  const int cx = x + kCellSize / 2;
  const int cy = y + kCellSize / 2 + 2;

  const SDL_Color body = piece > 0 ? theme.white_piece : theme.black_piece;
  const SDL_Color outline = piece > 0 ? theme.white_outline : theme.black_outline;
  const SDL_Color label = piece > 0 ? theme.black_piece : theme.white_piece;

  if (selected) {
    fill_rect(renderer, {x + 4, y + 4, kCellSize - 8, kCellSize - 8}, theme.accent);
  }

  fill_circle(renderer, cx, cy + 8, 12, outline);
  fill_rect(renderer, {cx - 12, cy + 8, 24, 6}, outline);
  fill_circle(renderer, cx, cy + 5, 10, body);
  fill_rect(renderer, {cx - 10, cy + 7, 20, 4}, body);

  switch (piece_type(piece)) {
    case kPawn:
      fill_circle(renderer, cx, cy - 8, 6, outline);
      fill_circle(renderer, cx, cy - 9, 5, body);
      break;
    case kKnight:
      fill_rect(renderer, {cx - 8, cy - 10, 12, 14}, outline);
      fill_rect(renderer, {cx - 7, cy - 9, 10, 12}, body);
      fill_circle(renderer, cx + 3, cy - 8, 5, outline);
      fill_circle(renderer, cx + 3, cy - 8, 4, body);
      break;
    case kBishop:
      fill_circle(renderer, cx, cy - 10, 6, outline);
      fill_circle(renderer, cx, cy - 10, 5, body);
      fill_rect(renderer, {cx - 4, cy - 10, 8, 16}, body);
      break;
    case kRook:
      fill_rect(renderer, {cx - 10, cy - 12, 20, 14}, outline);
      fill_rect(renderer, {cx - 9, cy - 11, 18, 12}, body);
      break;
    case kQueen:
      fill_circle(renderer, cx - 7, cy - 10, 3, outline);
      fill_circle(renderer, cx, cy - 13, 3, outline);
      fill_circle(renderer, cx + 7, cy - 10, 3, outline);
      fill_circle(renderer, cx - 7, cy - 10, 2, body);
      fill_circle(renderer, cx, cy - 13, 2, body);
      fill_circle(renderer, cx + 7, cy - 10, 2, body);
      fill_rect(renderer, {cx - 9, cy - 9, 18, 12}, body);
      break;
    case kKing:
      fill_rect(renderer, {cx - 3, cy - 16, 6, 18}, outline);
      fill_rect(renderer, {cx - 9, cy - 10, 18, 12}, body);
      fill_rect(renderer, {cx - 8, cy - 16, 16, 4}, outline);
      fill_rect(renderer, {cx - 2, cy - 20, 4, 8}, outline);
      break;
    default:
      break;
  }

  draw_text(renderer, std::string(1, piece_symbol(piece)), cx, y + 12, 2, label, true);
}

void draw_board(SDL_Renderer* renderer, const Theme& theme, const GameState& game) {
  fill_rect(renderer, {kBoardX - 8, kBoardY - 8, kCellSize * kBoardSize + 16, kCellSize * kBoardSize + 16},
            theme.frame);
  fill_rect(renderer, {kBoardX - 4, kBoardY - 4, kCellSize * kBoardSize + 8, kCellSize * kBoardSize + 8},
            theme.panel);

  const auto selected_moves = game.selected >= 0 ? moves_for_square(game.legal_moves, game.selected)
                                                 : std::vector<Move>{};
  for (int sq = 0; sq < 64; ++sq) {
    const int file = file_of(sq);
    const int rank = rank_of(sq);
    SDL_Rect square{kBoardX + file * kCellSize, kBoardY + rank * kCellSize, kCellSize, kCellSize};
    fill_rect(renderer, square, ((file + rank) % 2 == 0) ? theme.light_square : theme.dark_square);

    if (sq == game.cursor) {
      draw_rect(renderer, {square.x + 1, square.y + 1, square.w - 2, square.h - 2}, theme.cursor);
      draw_rect(renderer, {square.x + 2, square.y + 2, square.w - 4, square.h - 4}, theme.cursor);
    }
    if (sq == game.selected) {
      fill_rect(renderer, {square.x + 5, square.y + 5, square.w - 10, square.h - 10},
                SDL_Color{theme.accent.r, theme.accent.g, theme.accent.b, 72});
    }
  }

  for (const auto& move : selected_moves) {
    const int cx = kBoardX + file_of(move.to) * kCellSize + kCellSize / 2;
    const int cy = kBoardY + rank_of(move.to) * kCellSize + kCellSize / 2;
    fill_circle(renderer, cx, cy, 6, theme.move_hint);
  }

  for (int sq = 0; sq < 64; ++sq) {
    if (game.board[sq] != 0) {
      draw_piece(renderer, theme, game.board[sq], sq, sq == game.selected);
    }
  }
}

std::string current_status_text(const GameState& game) {
  if (game.game_over) {
    return game.draw ? "DRAW" : (game.winner > 0 ? "WHITE WINS" : "BLACK WINS");
  }
  const int king_sq = find_king(game.board, game.white_turn);
  if (king_sq >= 0 && is_square_attacked(game, king_sq, !game.white_turn)) {
    return "CHECK";
  }
  if (!game.white_turn && game.versus_cpu) {
    return "CPU THINK";
  }
  return "READY";
}

void draw_sidebar(SDL_Renderer* renderer, const Theme& theme, const GameState& game) {
  const int left_x = kPanelX + 12;
  const int right_x = kPanelX + kPanelW - 12;
  fill_rect(renderer, {kPanelX + 6, kPanelY + 8, kPanelW, kPanelH}, theme.frame);
  fill_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, theme.panel);
  draw_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, theme.muted);

  draw_text(renderer, "TURN", left_x, kPanelY + 16, 1, theme.muted, false);
  draw_text_right(renderer, game.white_turn ? "WHITE" : "BLACK", right_x, kPanelY + 30, 2,
                  theme.text);

  draw_text(renderer, "STATUS", left_x, kPanelY + 58, 1, theme.muted, false);
  draw_text_right(renderer, current_status_text(game), right_x, kPanelY + 72, 2, theme.text);

  draw_text(renderer, "MOVE", left_x, kPanelY + 100, 1, theme.muted, false);
  draw_text_right(renderer, std::to_string(1 + game.ply_count / 2), right_x, kPanelY + 114, 2,
                  theme.text);

  fill_rect(renderer, {kPanelX + 10, kPanelY + 140, kPanelW - 20, 82}, theme.panel_highlight);
  draw_text(renderer, "A SELECT", kPanelX + 18, kPanelY + 154, 1, theme.text, false);
  draw_text(renderer, "B CANCEL", kPanelX + 18, kPanelY + 170, 1, theme.text, false);
  draw_text(renderer, "START PAUSE", kPanelX + 18, kPanelY + 186, 1, theme.text, false);
  draw_text(renderer, "SELECT EXIT", kPanelX + 18, kPanelY + 202, 1, theme.text, false);

  draw_text(renderer, "RULES", left_x, kPanelY + 238, 2, theme.muted, false);
  draw_text(renderer, "CASTLING", kPanelX + 18, kPanelY + 268, 1, theme.text, false);
  draw_text(renderer, "EN PASSANT", kPanelX + 18, kPanelY + 284, 1, theme.text, false);
  draw_text(renderer, "PROMOTION", kPanelX + 18, kPanelY + 300, 1, theme.text, false);
  draw_text(renderer, "CPU DEPTH 4", kPanelX + 18, kPanelY + 316, 1, theme.muted, false);
}

void draw_overlay(SDL_Renderer* renderer, const Theme& theme, const std::string& title, const std::string& subtitle) {
  fill_rect(renderer, {78, 208, 356, 92}, theme.overlay);
  draw_rect(renderer, {78, 208, 356, 92}, theme.frame);
  draw_text(renderer, title, kWindowWidth / 2, 224, 4, theme.text, true);
  draw_text(renderer, subtitle, kWindowWidth / 2, 260, 2, theme.muted, true);
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

void draw_promotion_overlay(SDL_Renderer* renderer, const Theme& theme, const GameState& game) {
  fill_rect(renderer, {70, 194, 372, 120}, theme.overlay);
  draw_rect(renderer, {70, 194, 372, 120}, theme.frame);
  draw_text(renderer, "PROMOTE PAWN", kWindowWidth / 2, 208, 3, theme.text, true);
  draw_text(renderer, "LEFT RIGHT TO CHOOSE / A TO CONFIRM", kWindowWidth / 2, 236, 1, theme.muted, true);

  constexpr std::array<int, 4> kPromotions{{kQueen, kRook, kBishop, kKnight}};
  for (int index = 0; index < 4; ++index) {
    SDL_Rect card{116 + index * 74, 254, 54, 40};
    fill_rect(renderer, card, index == game.promotion_index ? theme.accent : theme.panel_highlight);
    draw_rect(renderer, card, theme.frame);
    draw_text(renderer, std::string(1, piece_symbol(kPromotions[index])), card.x + card.w / 2, card.y + 8, 3,
              index == game.promotion_index ? theme.black_piece : theme.text, true);
  }
}

bool move_targets_square(const Move& move, int square) {
  return move.to == square;
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
  if (pp_init(&context, "pixelpal-chess") != 0) {
    std::fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }
  pp_get_framebuffer_size(&context, &width, &height);
  width = std::max(width, 512);
  height = std::max(height, 512);

  window = SDL_CreateWindow("Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
        trigger_tone(tone, 760.0f, 80);
      }
    } else if (pressed_start) {
      if (game.game_over) {
        reset_game(game);
        trigger_tone(tone, 760.0f, 80);
      } else if (!game.promotion_pending) {
        game.paused = !game.paused;
        trigger_tone(tone, game.paused ? 240.0f : 640.0f, 52);
      }
    }

    if (!game.mode_select_active && !game.paused && !game.game_over) {
      if (game.promotion_pending) {
        if (pressed_left) {
          game.promotion_index = (game.promotion_index + 3) % 4;
          trigger_tone(tone, 430.0f, 18);
        }
        if (pressed_right) {
          game.promotion_index = (game.promotion_index + 1) % 4;
          trigger_tone(tone, 430.0f, 18);
        }
        if (pressed_b) {
          game.promotion_pending = false;
          game.promotion_moves.clear();
          game.promotion_index = 0;
          trigger_tone(tone, 320.0f, 24);
        }
        if (pressed_a && !game.promotion_moves.empty()) {
          const Move chosen = game.promotion_moves[game.promotion_index];
          game = apply_move(game, chosen);
          refresh_game_state(game);
          if (game.versus_cpu && !game.white_turn && !game.game_over) {
            game.ai_ready_at = now + kAiDelayMs;
          } else {
            game.ai_ready_at = 0U;
          }
          trigger_tone(tone, 860.0f, 40);
        }
      } else if (game.white_turn || !game.versus_cpu) {
        int file = file_of(game.cursor);
        int rank = rank_of(game.cursor);
        if (pressed_left) {
          file = std::max(0, file - 1);
          game.cursor = make_sq(file, rank);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_right) {
          file = std::min(7, file + 1);
          game.cursor = make_sq(file, rank);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_up) {
          rank = std::max(0, rank - 1);
          game.cursor = make_sq(file_of(game.cursor), rank);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_down) {
          rank = std::min(7, rank + 1);
          game.cursor = make_sq(file_of(game.cursor), rank);
          trigger_tone(tone, 420.0f, 18);
        }
        if (pressed_b) {
          game.selected = -1;
          trigger_tone(tone, 320.0f, 24);
        }
        if (pressed_a) {
          bool handled = false;
          if (game.selected >= 0) {
            std::vector<Move> matching;
            for (const auto& move : game.legal_moves) {
              if (move.from == game.selected && move_targets_square(move, game.cursor)) {
                matching.push_back(move);
              }
            }
            if (!matching.empty()) {
              if (matching.size() > 1 && matching.front().promotion != 0) {
                game.promotion_pending = true;
                game.promotion_moves = matching;
                game.promotion_index = 0;
                trigger_tone(tone, 680.0f, 28);
              } else {
                game = apply_move(game, matching.front());
                refresh_game_state(game);
                if (game.versus_cpu && !game.white_turn && !game.game_over) {
                  game.ai_ready_at = now + kAiDelayMs;
                } else {
                  game.ai_ready_at = 0U;
                }
                trigger_tone(tone, matching.front().promotion != 0 ? 900.0f : 760.0f, 38);
              }
              handled = true;
            }
          }
          const int piece = game.board[game.cursor];
          const bool current_player_piece = game.white_turn ? piece > 0 : piece < 0;
          if (!handled && current_player_piece && square_has_legal_move(game, game.cursor)) {
            game.selected = game.cursor;
            trigger_tone(tone, 560.0f, 28);
          }
        }
      } else if (game.versus_cpu && now >= game.ai_ready_at) {
        if (!game.legal_moves.empty()) {
          const Move best = choose_ai_move(game);
          game = apply_move(game, best);
          refresh_game_state(game);
          trigger_tone(tone, best.promotion != 0 ? 240.0f : 320.0f, 52);
        }
      }
    }

    fill_rect(renderer, {0, 0, kWindowWidth, kWindowHeight}, theme.background);
    draw_text(renderer, "CHESS", kWindowWidth / 2, 18, 6, theme.text, true);
    draw_text(renderer,
              game.versus_cpu ? "FULL RULES / CASTLING / EN PASSANT / CPU OPPONENT"
                              : "FULL RULES / CASTLING / EN PASSANT / 2 PLAYER",
              kWindowWidth / 2, 62, 1, theme.muted,
              true);
    draw_board(renderer, theme, game);
    draw_sidebar(renderer, theme, game);

    if (game.mode_select_active) {
      draw_mode_overlay(renderer, theme, game.mode_index);
    } else if (game.promotion_pending) {
      draw_promotion_overlay(renderer, theme, game);
    } else if (game.paused && !game.game_over) {
      draw_overlay(renderer, theme, "PAUSED", "START TO RESUME");
    } else if (game.game_over) {
      const std::string title = game.draw ? "DRAW" : (game.winner > 0 ? "WHITE WINS" : "BLACK WINS");
      draw_overlay(renderer, theme, title, game.end_message.empty() ? "START FOR NEW MATCH" : game.end_message);
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
