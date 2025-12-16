#include "Chess.h"
#include <cmath>
#include <limits>

Chess::Chess() { _grid = new Grid(8, 8); }

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
  FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

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
  // need to implement friendly/unfriendly in bit so for now this hack
  int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
  int pieceColor = bit.gameTag() & 128;
  if (pieceColor == currentPlayer)
    return true;
  return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
  return true;
}

void Chess::stopGame() {
  _grid->forEachSquare(
      [](ChessSquare *square, int x, int y) { square->destroyBit(); });
}

Player *Chess::ownerAt(int x, int y) const {
  if (x < 0 || x >= 8 || y < 0 || y >= 8) {
    return nullptr;
  }

  auto square = _grid->getSquare(x, y);
  if (!square || !square->bit()) {
    return nullptr;
  }
  return square->bit()->getOwner();
}

Player *Chess::checkForWinner() { return nullptr; }

bool Chess::checkForDraw() { return false; }

std::string Chess::initialStateString() { return stateString(); }

std::string Chess::stateString() {
  std::string s;
  s.reserve(64);
  _grid->forEachSquare(
      [&](ChessSquare *square, int x, int y) { s += pieceNotation(x, y); });
  return s;
}

void Chess::setStateString(const std::string &s) {
  _grid->forEachSquare([&](ChessSquare *square, int x, int y) {
    int index = y * 8 + x;
    char playerNumber = s[index] - '0';
    if (playerNumber) {
      square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
    } else {
      square->setBit(nullptr);
    }
  });
}
