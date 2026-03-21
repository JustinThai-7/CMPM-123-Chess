#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;

    bool gameHasAI() override;
    void updateAI() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

    // The single move-generation entry point.
    // Syncs _boardArray from the grid, generates pseudo-legal moves via bitboards,
    // then filters out any move that would leave the mover's king in check.
    std::vector<BitMove> generateAllMoves(int player);

    // Bitboard snapshot built from _boardArray (used by AI search).
    MoveGen::BoardState getBoardStateFromInternal() const;

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    Grid* _grid;
    int _boardArray[64];
    bool _whiteToMoveInternal;

    void buildInternalBoardFromGrid();
    void syncGridFromInternalBoard();
    void applyMoveToInternalBoard(const BitMove &m, int &captured, int &originalFromTag, int &epCapSq, int &savedEP);
    void undoMoveInInternalBoard(const BitMove &m, int captured, int originalFromTag, int epCapSq, int savedEP);
    bool isSquareAttackedInternal(int sq, int attackerPlayer) const;
    int materialScore();
    int negamax(int player, int depth, int alpha, int beta);

    // Executes a move on the UI grid and handles all bookkeeping (AI path).
    void makeMoveOnGrid(const BitMove &m, ChessPiece promotion = NoPiece);
    // Shared post-move bookkeeping: piece count, half-move clock, EP square,
    // internal board sync, turn end, and position-history recording.
    void postMoveBookkeeping(ChessSquare *srcSq, ChessSquare *dstSq, Bit &movedBit);

    int _halfmoveClock;
    int _enPassantSquare; // target square for en passant capture, or -1
    int _countMoves;   // counts search nodes visited per AI turn (diagnostic)
    int _prevPieceCount;  // piece count at start of last turn, for capture detection
    std::unordered_map<std::string, int> _positionHistory; // board+side-to-move → count
};