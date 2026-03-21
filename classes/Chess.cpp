#include "Chess.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

Chess::Chess() {
    _grid = new Grid(8, 8);
    for (int i = 0; i < 64; ++i) _boardArray[i] = 0;
    _whiteToMoveInternal = true;
    _halfmoveClock = 0;
    _enPassantSquare = -1;
    _prevPieceCount = 32;
    _countMoves = 0;
}

Chess::~Chess() { delete _grid; }

char Chess::pieceNotation(int x, int y) const {
  const char *wpieces = {"0PNBRQK"};
  const char *bpieces = {"0pnbrqk"};
  Bit *bit = _grid->getSquare(x, y)->bit();
  char notation = '0';
  if (bit) {
    notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()]
                                    : bpieces[bit->gameTag() - 128];
  }
  return notation;
}

Bit *Chess::PieceForPlayer(const int playerNumber, ChessPiece piece) {
  const char *pieces[] = {"pawn.png", "knight.png", "bishop.png",
                          "rook.png", "queen.png",  "king.png"};

  Bit *bit = new Bit();
  // should possibly be cached from player class?
  const char *pieceName = pieces[piece - 1];
  std::string spritePath =
      std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
  bit->LoadTextureFromFile(spritePath.c_str());
  bit->setOwner(getPlayerAt(playerNumber));
  bit->setSize(pieceSize, pieceSize);

  return bit;
}

void Chess::setUpBoard() {
  setNumberOfPlayers(2);
  _gameOptions.rowX = 8;
  _gameOptions.rowY = 8;

  _grid->initializeChessSquares(pieceSize, "boardsquare.png");
  _halfmoveClock = 0;
  _enPassantSquare = -1;
  _prevPieceCount = 32;
  _positionHistory.clear();
  initMagics();
  FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
  setAIPlayer(1);  // black is AI
  startGame();
}

void Chess::FENtoBoard(const std::string &fen) {
  // convert a FEN string to a board
  // FEN is a space delimited string with 6 fields
  // 1: piece placement (from white's perspective)
  // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
  // ARE BELOW
  // 2: active color (W or B)
  // 3: castling availability (KQkq or -)
  // 4: en passant target square (in algebraic notation, or -)
  // 5: halfmove clock (number of halfmoves since the last capture or pawn
  // advance)

  // Extract just the board position portion (before any space)
  std::string boardPosition = fen;
  size_t spacePos = fen.find(' ');
  if (spacePos != std::string::npos) {
    boardPosition = fen.substr(0, spacePos);
  }

  // FEN starts from rank 8 (top of board) and goes to rank 1 (bottom)
  // Each rank is separated by '/'
  int row = 7; // Start from rank 8 (index 7 in 0-indexed array)
  int col = 0;

  for (char c : boardPosition) {
    if (c == '/') {
      // Move to next rank
      row--;
      col = 0;
    } else if (c >= '1' && c <= '8') {
      // Empty squares - skip that many columns
      col += (c - '0');
    } else {
      // It's a piece character
      int playerNumber =
          (c >= 'a' && c <= 'z')
              ? 1
              : 0; // lowercase = black (player 1), uppercase = white (player 0)
      ChessPiece piece = NoPiece;

      char lowerC = (c >= 'A' && c <= 'Z')
                        ? (c + 32)
                        : c; // Convert to lowercase for comparison

      switch (lowerC) {
      case 'p':
        piece = Pawn;
        break;
      case 'n':
        piece = Knight;
        break;
      case 'b':
        piece = Bishop;
        break;
      case 'r':
        piece = Rook;
        break;
      case 'q':
        piece = Queen;
        break;
      case 'k':
        piece = King;
        break;
      }

      if (piece != NoPiece && col < 8 && row >= 0) {
        Bit *bit = PieceForPlayer(playerNumber, piece);
        bit->setGameTag(
            piece + (playerNumber * 128)); // Set game tag with player offset
        ChessSquare *square = _grid->getSquare(col, row);
        square->setBit(bit);
        bit->setPosition(
            square->getPosition()); // Set piece position to match the square
      }
      col++;
    }
  }
}

bool Chess::actionForEmptyHolder(BitHolder &holder) { return false; }

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src) {
  Player *owner = bit.getOwner();
  Player *current = getCurrentPlayer();
  if (!owner || !current) return false;
  return owner->playerNumber() == current->playerNumber();
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
  ChessSquare *srcSq = static_cast<ChessSquare *>(&src);
  ChessSquare *dstSq = static_cast<ChessSquare *>(&dst);
  int fromIdx = srcSq->getRow() * 8 + srcSq->getColumn();
  int toIdx   = dstSq->getRow() * 8 + dstSq->getColumn();
  if (fromIdx == toIdx) return false;
  Player *owner = bit.getOwner();
  if (!owner) return false;
  int player = owner->playerNumber();
  buildInternalBoardFromGrid();
  for (auto &m : generateAllMoves(player))
    if (m.from == fromIdx && m.to == toIdx) return true;
  return false;
}



void Chess::stopGame() {
  _grid->forEachSquare(
      [](ChessSquare *square, int x, int y) { square->destroyBit(); });
}

// Internal board helpers

void Chess::buildInternalBoardFromGrid()
{
  for (int i=0;i<64;i++) _boardArray[i] = 0;
  _grid->forEachSquare([&](ChessSquare* square, int x, int y){
    int idx = square->getSquareIndex();
    if (square->bit()) _boardArray[idx] = square->bit()->gameTag();
  });
  Player* cur = getCurrentPlayer();
  _whiteToMoveInternal = (cur && cur->playerNumber() == 0);
}

void Chess::syncGridFromInternalBoard()
{
  for (int i=0;i<64;i++) {
    ChessSquare* sq = _grid->getSquareByIndex(i);
    if (!sq) continue;
    int tag = _boardArray[i];
    sq->destroyBit();
    if (tag != 0) {
      int playerNumber = tag < 128 ? 0 : 1;
      int pieceId = tag % 128;
      Bit* b = PieceForPlayer(playerNumber, static_cast<ChessPiece>(pieceId));
      sq->setBit(b);
      b->setParent(sq);
      b->moveTo(sq->getPosition());
      b->setPickedUp(false);
      b->setGameTag(tag);
    }
  }
}

void Chess::applyMoveToInternalBoard(const BitMove &m, int &captured, int &originalFromTag, int &epCapSq, int &savedEP)
{
  int from = m.from;
  int to = m.to;
  epCapSq = -1;
  savedEP = _enPassantSquare;
  if (from < 0 || from >= 64 || to < 0 || to >= 64) { captured = 0; originalFromTag = 0; return; }
  originalFromTag = _boardArray[from];
  captured = _boardArray[to];

  // En passant: pawn captures diagonally to an empty square.
  if ((originalFromTag % 128 == Pawn) && (from % 8 != to % 8) && (captured == 0)) {
    int mover = (originalFromTag >= 128) ? 1 : 0;
    epCapSq = to + (mover == 0 ? -8 : 8);
    captured = _boardArray[epCapSq];
    _boardArray[epCapSq] = 0;
  }

  if (m.promotion != NoPiece) {
    int playerNumber = (originalFromTag >= 128) ? 1 : 0;
    _boardArray[to] = (playerNumber == 0) ? m.promotion : (m.promotion + 128);
  } else {
    _boardArray[to] = originalFromTag;
  }
  _boardArray[from] = 0;

  // Update en passant square: set only on pawn double push.
  _enPassantSquare = -1;
  if ((originalFromTag % 128 == Pawn) && (from % 8 == to % 8)) {
    int mover = (originalFromTag >= 128) ? 1 : 0;
    if (mover == 0 && from / 8 == 1 && to / 8 == 3)
      _enPassantSquare = 2 * 8 + (from % 8);
    else if (mover == 1 && from / 8 == 6 && to / 8 == 4)
      _enPassantSquare = 5 * 8 + (from % 8);
  }

  _whiteToMoveInternal = !_whiteToMoveInternal;
}

void Chess::undoMoveInInternalBoard(const BitMove &m, int captured, int originalFromTag, int epCapSq, int savedEP)
{
  int from = m.from;
  int to = m.to;
  if (from < 0 || from >= 64 || to < 0 || to >= 64) return;
  _boardArray[from] = originalFromTag;
  if (epCapSq != -1) {
    _boardArray[to] = 0;
    _boardArray[epCapSq] = captured;
  } else {
    _boardArray[to] = captured;
  }
  _enPassantSquare = savedEP;
  _whiteToMoveInternal = !_whiteToMoveInternal;
}

// Piece-square tables (from white's perspective, row 0 = rank 1).
// Black mirrors by using square (63 - sq).

static const int pst_pawn[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10,-20,-20, 10, 10,  5,
   5, -5,-10,  0,  0,-10, -5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5,  5, 10, 25, 25, 10,  5,  5,
  10, 10, 20, 30, 30, 20, 10, 10,
  50, 50, 50, 50, 50, 50, 50, 50,
   0,  0,  0,  0,  0,  0,  0,  0,
};
static const int pst_knight[64] = {
  -50,-40,-30,-30,-30,-30,-40,-50,
  -40,-20,  0,  5,  5,  0,-20,-40,
  -30,  5, 10, 15, 15, 10,  5,-30,
  -30,  0, 15, 20, 20, 15,  0,-30,
  -30,  5, 15, 20, 20, 15,  5,-30,
  -30,  0, 10, 15, 15, 10,  0,-30,
  -40,-20,  0,  0,  0,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int pst_bishop[64] = {
  -20,-10,-10,-10,-10,-10,-10,-20,
  -10,  5,  0,  0,  0,  0,  5,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int pst_rook[64] = {
   0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0,
};
static const int pst_queen[64] = {
  -20,-10,-10, -5, -5,-10,-10,-20,
  -10,  0,  5,  0,  0,  0,  0,-10,
  -10,  5,  5,  5,  5,  5,  0,-10,
    0,  0,  5,  5,  5,  5,  0, -5,
   -5,  0,  5,  5,  5,  5,  0, -5,
  -10,  0,  5,  5,  5,  5,  0,-10,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20,
};
static const int pst_king[64] = {
   20, 30, 10,  0,  0, 10, 30, 20,
   20, 20,  0,  0,  0,  0, 20, 20,
  -10,-20,-20,-20,-20,-20,-20,-10,
  -20,-30,-30,-40,-40,-30,-30,-20,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
};

int Chess::materialScore()
{
  int whiteScore = 0;
  int blackScore = 0;
  for (int i = 0; i < 64; i++) {
    int tag = _boardArray[i];
    if (tag == 0) continue;
    bool white = tag < 128;
    int id = tag % 128;
    int base = 0;
    const int* pst = nullptr;
    switch (id) {
      case Pawn:   base = 100;   pst = pst_pawn;   break;
      case Knight: base = 320;   pst = pst_knight; break;
      case Bishop: base = 330;   pst = pst_bishop; break;
      case Rook:   base = 500;   pst = pst_rook;   break;
      case Queen:  base = 900;   pst = pst_queen;  break;
      case King:   base = 20000; pst = pst_king;   break;
      default: continue;
    }

    int sq = white ? i : (63 - i);
    int val = base + pst[sq];
    if (white) whiteScore += val; else blackScore += val;
  }
  return whiteScore - blackScore;
}

bool Chess::gameHasAI() {
  return _gameOptions.AIPlaying;
}

// negamax

int Chess::negamax(int player, int depth, int alpha, int beta) {
  ++_countMoves;

  if (depth == 0)
    return materialScore() * (player == 0 ? 1 : -1);

  // Generate legal moves
  auto moves = generateAllMoves(player);

  // Terminal: no legal moves.
  if (moves.empty()) {
    int kingSq = -1;
    for (int i = 0; i < 64; ++i) {
      int t = _boardArray[i];
      if (t && ((t >= 128) ? 1 : 0) == player && (t % 128) == King) { kingSq = i; break; }
    }
    // Checkmate: shorter mates score higher.
    if (kingSq != -1 && isSquareAttackedInternal(kingSq, 1 - player))
      return -(20000 + depth);
    return 0;  // Stalemate.
  }

  // Move ordering: captures first
  std::sort(moves.begin(), moves.end(), [&](const BitMove& a, const BitMove& b) {
    return (_boardArray[a.to] != 0) > (_boardArray[b.to] != 0);
  });

  int bestVal = -1000000;
  for (auto& m : moves) {
    int cap = 0, orig = 0, epCapSq = 0, savedEP = 0;
    applyMoveToInternalBoard(m, cap, orig, epCapSq, savedEP);
    bestVal = std::max(bestVal, -negamax(1 - player, depth - 1, -beta, -alpha));
    undoMoveInInternalBoard(m, cap, orig, epCapSq, savedEP);
    alpha = std::max(alpha, bestVal);
    if (alpha >= beta) break;
  }
  return bestVal;
}

//called every frame during AI's turn
void Chess::updateAI() {
  if (!gameHasAI()) return;
  Player* cur = getCurrentPlayer();
  if (!cur || !cur->isAIPlayer()) return;

  buildInternalBoardFromGrid();
  int player = cur->playerNumber();

  auto moves = generateAllMoves(player);
  if (moves.empty()) return;

  std::sort(moves.begin(), moves.end(), [&](const BitMove& a, const BitMove& b) {
    return (_boardArray[a.to] != 0) > (_boardArray[b.to] != 0);
  });

  _countMoves = 0;
  constexpr int INF = 1000000;
  int bestVal = -INF;
  BitMove bestMove = moves[0];

  for (auto& m : moves) {
    int cap = 0, orig = 0, epCapSq = 0, savedEP = 0;
    applyMoveToInternalBoard(m, cap, orig, epCapSq, savedEP);
    int val = -negamax(1 - player, 4, -INF, INF);
    undoMoveInInternalBoard(m, cap, orig, epCapSq, savedEP);
    if (val > bestVal) {
      bestVal = val;
      bestMove = m;
    }
  }

  std::cout << "AI (player " << player << ") checked " << _countMoves << " positions\n";

  makeMoveOnGrid(bestMove);
}

Player *Chess::checkForWinner() {
  buildInternalBoardFromGrid();
  Player *current = getCurrentPlayer();
  if (!current) return nullptr;
  int player = current->playerNumber();

  if (generateAllMoves(player).empty()) {
    // Find king square
    int kingSq = -1;
    for (int i = 0; i < 64; ++i) {
      int t = _boardArray[i];
      if (t && ((t >= 128) ? 1 : 0) == player && (t % 128) == King) { kingSq = i; break; }
    }
    // Checkmate: no legal moves AND king is in check
    if (kingSq != -1 && isSquareAttackedInternal(kingSq, 1 - player))
      return getPlayerAt(1 - player);
  }
  return nullptr;
}

bool Chess::checkForDraw() {
  buildInternalBoardFromGrid();
  Player *current = getCurrentPlayer();
  if (!current) return false;
  int player = current->playerNumber();

  // Stalemate: no legal moves and king not checked
  if (generateAllMoves(player).empty()) {
    int kingSq = -1;
    for (int i = 0; i < 64; ++i) {
      int t = _boardArray[i];
      if (t && ((t >= 128) ? 1 : 0) == player && (t % 128) == King) { kingSq = i; break; }
    }
    if (kingSq == -1 || !isSquareAttackedInternal(kingSq, 1 - player))
      return true;
  }

  // 50 move rule
  if (_halfmoveClock >= 100) return true;

  // Threefold repetition
  std::string pos = stateString() + (_gameOptions.currentTurnNo % 2 == 0 ? "w" : "b");
  if (_positionHistory.count(pos) && _positionHistory.at(pos) >= 3) return true;

  return false;
}

std::string Chess::initialStateString() { return stateString(); }

std::string Chess::stateString() {
  std::string s;
  s.reserve(64);
  _grid->forEachSquare(
      [&](ChessSquare *square, int x, int y) { s += pieceNotation(x, y); });
  return s;
}

void Chess::setStateString(const std::string &s) {
  // Expecting a 64-char board string matching `stateString()` format:
  // '0' = empty, 'P/N/B/R/Q/K' = white, 'p/n/b/r/q/k' = black
  if ((int)s.size() < 64) return;
  _grid->forEachSquare([&](ChessSquare *square, int x, int y) {
    int index = y * 8 + x;
    char c = s[index];
    if (c == '0') {
      square->destroyBit();
      return;
    }
    int playerNumber = (c >= 'a' && c <= 'z') ? 1 : 0;
    char lc = (c >= 'A' && c <= 'Z') ? (c + 32) : c; // lowercase for switch
    ChessPiece piece = NoPiece;
    switch (lc) {
      case 'p': piece = Pawn; break;
      case 'n': piece = Knight; break;
      case 'b': piece = Bishop; break;
      case 'r': piece = Rook; break;
      case 'q': piece = Queen; break;
      case 'k': piece = King; break;
      default: piece = NoPiece; break;
    }
    square->destroyBit();
    if (piece != NoPiece) {
      Bit* b = PieceForPlayer(playerNumber, piece);
      square->setBit(b);
      b->setParent(square);
      b->moveTo(square->getPosition());
      b->setPickedUp(false);
      b->setGameTag(piece + (playerNumber * 128));
    }
  });
}

MoveGen::BoardState Chess::getBoardStateFromInternal() const {
  MoveGen::BoardState state;
  for (int i = 0; i < 64; ++i) {
    int tag = _boardArray[i];
    if (tag == 0) continue;
    int color = (tag >= 128) ? 1 : 0;
    ChessPiece type = static_cast<ChessPiece>(tag % 128);
    if (type != NoPiece) state.set(color, type, i);
  }
  return state;
}

// Return true if square `sq` is attacked by any piece of `attackerPlayer`.
bool Chess::isSquareAttackedInternal(int sq, int attackerPlayer) const {
  if (sq < 0 || sq >= 64) return false;
  int sx = sq % 8;
  int sy = sq / 8;

  // Pawn attacks
  if (attackerPlayer == 0) { // white pawns attack from below (y-1)
    int py = sy - 1;
    if (py >= 0) {
      for (int dx : {-1, 1}) {
        int nx = sx + dx;
        if (nx < 0 || nx >= 8) continue;
        int idx = py * 8 + nx;
        int tag = _boardArray[idx];
        if (tag != 0 && (tag < 128) && (tag % 128) == Pawn) return true;
      }
    }
  } else { // black pawns attack from above (y+1)
    int py = sy + 1;
    if (py < 8) {
      for (int dx : {-1, 1}) {
        int nx = sx + dx;
        if (nx < 0 || nx >= 8) continue;
        int idx = py * 8 + nx;
        int tag = _boardArray[idx];
        if (tag != 0 && (tag >= 128) && (tag % 128) == Pawn) return true;
      }
    }
  }

  // Knight attacks
  static const int kn[8][2] = {{1,2},{1,-2},{-1,2},{-1,-2},{2,1},{2,-1},{-2,1},{-2,-1}};
  for (auto &o : kn) {
    int nx = sx + o[0];
    int ny = sy + o[1];
    if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) continue;
    int tag = _boardArray[ny * 8 + nx];
    if (tag == 0) continue;
    int color = (tag >= 128) ? 1 : 0;
    if (color == attackerPlayer && (tag % 128) == Knight) return true;
  }

  // King (adjacent)
  for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
    if (dx == 0 && dy == 0) continue;
    int nx = sx + dx, ny = sy + dy;
    if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8) continue;
    int tag = _boardArray[ny * 8 + nx];
    if (tag == 0) continue;
    int color = (tag >= 128) ? 1 : 0;
    if (color == attackerPlayer && (tag % 128) == King) return true;
  }

  // Sliding pieces: directions
  static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  for (int d = 0; d < 8; ++d) {
    int dx = dirs[d][0], dy = dirs[d][1];
    int cx = sx + dx, cy = sy + dy;
    while (cx >= 0 && cx < 8 && cy >= 0 && cy < 8) {
      int tag = _boardArray[cy * 8 + cx];
      if (tag != 0) {
        int color = (tag >= 128) ? 1 : 0;
        if (color == attackerPlayer) {
          int piece = tag % 128;
          bool diag = (dx != 0 && dy != 0);
          if (piece == Queen) return true;
          if (!diag && piece == Rook) return true;
          if (diag && piece == Bishop) return true;
        }
        break;
      }
      cx += dx; cy += dy;
    }
  }

  return false;
}

// ---------------------------------------------------------------------------
// GenerateAllMoves: THE single move-gen entry point.
// Builds a BoardState from _boardArray, generates pseudo-legal moves via
// bitboard generators, then filters out moves leaving own king in check.
// ---------------------------------------------------------------------------
std::vector<BitMove> Chess::generateAllMoves(int player) {
  MoveGen::BoardState state = getBoardStateFromInternal();

  std::vector<BitMove> pseudo;
  pseudo.reserve(64);
  MoveGen::generateAllPseudoLegalMoves(state, player, pseudo);

  // En passant pseudo-moves.
  if (_enPassantSquare != -1) {
    int epFile = _enPassantSquare % 8;
    int epRow  = _enPassantSquare / 8;
    int expectedRow  = (player == 0) ? 5 : 2; // EP square row for each color
    int capturingRow = (player == 0) ? 4 : 3; // row the capturing pawn sits on
    if (epRow == expectedRow) {
      for (int df : {-1, 1}) {
        int srcFile = epFile + df;
        if (srcFile < 0 || srcFile >= 8) continue;
        int srcSq = capturingRow * 8 + srcFile;
        int tag = _boardArray[srcSq];
        if (tag == 0) continue;
        if (((tag >= 128) ? 1 : 0) == player && (tag % 128) == Pawn)
          pseudo.emplace_back(srcSq, _enPassantSquare, Pawn);
      }
    }
  }

  std::vector<BitMove> legal;
  legal.reserve(pseudo.size());
  for (auto &m : pseudo) {
    int captured = 0, originalFrom = 0, epCapSq = 0, savedEP = 0;
    applyMoveToInternalBoard(m, captured, originalFrom, epCapSq, savedEP);

    int kingSq = -1;
    for (int i = 0; i < 64; ++i) {
      int t = _boardArray[i];
      if (t && ((t >= 128) ? 1 : 0) == player && (t % 128) == King) { kingSq = i; break; }
    }
    bool inCheck = (kingSq != -1) && isSquareAttackedInternal(kingSq, 1 - player);

    undoMoveInInternalBoard(m, captured, originalFrom, epCapSq, savedEP);
    if (!inCheck) legal.push_back(m);
  }

  return legal;
}

// ---------------------------------------------------------------------------
// makeMoveOnGrid: THE single path for applying any move to the UI grid.
// Handles captures, en-passant removal, promotion, and all post-move
// bookkeeping (half-move clock, EP square, internal board sync, turn end).
// Both AI (updateAI) and human (bitMovedFromTo) funnel through here.
// ---------------------------------------------------------------------------
void Chess::makeMoveOnGrid(const BitMove &m, ChessPiece promotion) {
  ChessSquare* srcSq = _grid->getSquareByIndex(m.from);
  ChessSquare* dstSq = _grid->getSquareByIndex(m.to);
  if (!srcSq || !dstSq) return;
  Bit* piece = srcSq->bit();
  if (!piece) return;

  bool isPawnMove = (piece->gameTag() % 128 == Pawn);

  // --- 1. En-passant: remove captured pawn from its real square ---
  if (isPawnMove && srcSq->getColumn() != dstSq->getColumn() && !dstSq->bit()) {
    ChessSquare* capSq = _grid->getSquare(dstSq->getColumn(), srcSq->getRow());
    if (capSq && capSq->bit()) capSq->destroyBit();
  }

  // --- 2. Normal capture ---
  if (dstSq->bit()) dstSq->destroyBit();

  // --- 3. Promotion (auto-queen if promotion enum not specified) ---
  ChessPiece promPiece = (promotion != NoPiece) ? promotion :
                         (isPawnMove && dstSq->getRow() == ((piece->getOwner() && piece->getOwner()->playerNumber()==0) ? 7 : 0))
                             ? Queen : NoPiece;

  if (promPiece != NoPiece && piece->getOwner()) {
    int playerNum = piece->getOwner()->playerNumber();
    Player* owner = piece->getOwner();
    srcSq->destroyBit();  // frees pawn
    Bit* promoted = PieceForPlayer(playerNum, promPiece);
    promoted->setGameTag(promPiece + (playerNum * 128));
    promoted->setOwner(owner);
    promoted->moveTo(dstSq->getPosition());
    dstSq->setBit(promoted);
    promoted->setPickedUp(false);
    piece = promoted;  // use promoted bit for bookkeeping below
    isPawnMove = true; // counts as pawn move for half-move clock
  } else {
    // --- 4. Normal transfer ---
    dstSq->setBit(piece);
    piece->moveTo(dstSq->getPosition());
    piece->setPickedUp(false);
  }

  // --- 5. Shared post-move bookkeeping ---
  postMoveBookkeeping(srcSq, dstSq, *piece);
}

// ---------------------------------------------------------------------------
// postMoveBookkeeping: called after a move is fully applied to the grid.
// Updates half-move clock, EP square, internal board, turn, and history.
// ---------------------------------------------------------------------------
void Chess::postMoveBookkeeping(ChessSquare *srcSq, ChessSquare *dstSq, Bit &movedBit) {
  bool isPawnMove = (movedBit.gameTag() % 128 == Pawn);

  int afterCount = 0;
  _grid->forEachSquare([&](ChessSquare* sq, int, int) { if (sq->bit()) ++afterCount; });
  bool isCapture = (afterCount < _prevPieceCount);
  if (isCapture || isPawnMove) _halfmoveClock = 0; else ++_halfmoveClock;
  _prevPieceCount = afterCount;

  // Update en passant square (set only on pawn double push).
  _enPassantSquare = -1;
  if (isPawnMove && movedBit.getOwner()) {
    int playerNum = movedBit.getOwner()->playerNumber();
    int fromRow = srcSq->getRow();
    int toRow   = dstSq->getRow();
    int file    = dstSq->getColumn();
    if (playerNum == 0 && fromRow == 1 && toRow == 3)
      _enPassantSquare = 2 * 8 + file;
    else if (playerNum == 1 && fromRow == 6 && toRow == 4)
      _enPassantSquare = 5 * 8 + file;
  }

  buildInternalBoardFromGrid();
  endTurn();

  std::string pos = stateString() + (_gameOptions.currentTurnNo % 2 == 0 ? "w" : "b");
  _positionHistory[pos]++;
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
  ChessSquare* srcSq = static_cast<ChessSquare*>(&src);
  ChessSquare* dstSq = static_cast<ChessSquare*>(&dst);
  bool isPawnMove = (bit.gameTag() % 128 == Pawn);

  // En passant
  if (isPawnMove && srcSq->getColumn() != dstSq->getColumn()) {
    int toIdx = dstSq->getRow() * 8 + dstSq->getColumn();
    if (toIdx == _enPassantSquare) {
      ChessSquare* capSq = _grid->getSquare(dstSq->getColumn(), srcSq->getRow());
      if (capSq && capSq->bit()) capSq->destroyBit();
    }
  }

  // Pawn promotion (auto-queen if move ends on back rank)
  if (isPawnMove && bit.getOwner()) {
    int playerNum = bit.getOwner()->playerNumber();
    int backRank  = (playerNum == 0) ? 7 : 0;
    if (dstSq->getRow() == backRank) {
      bit.setGameTag(Queen + (playerNum * 128));
      std::string spritePath = std::string(playerNum == 0 ? "w_" : "b_") + "queen.png";
      bit.LoadTextureFromFile(spritePath.c_str());
    }
  }

  postMoveBookkeeping(srcSq, dstSq, bit);
}
