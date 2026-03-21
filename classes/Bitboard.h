#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <iostream>
#include <vector>
#include "MagicBitboards.h"

enum ChessPiece
{
    NoPiece,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

class BitboardElement {
  public:
    // Constructors
    BitboardElement()
        : _data(0) { }
    BitboardElement(uint64_t data)
        : _data(data) { }

    // Getters and Setters
    uint64_t getData() const { return _data; }
    void setData(uint64_t data) { _data = data; }

    // Method to loop through each bit in the element and perform an operation on it.
    template <typename Func>
    void forEachBit(Func func) const {
        if (_data != 0) {
            uint64_t tempData = _data;
            while (tempData) {
                int index = bitScanForward(tempData);
                func(index);
                tempData &= tempData - 1;
            }
        }
    }

    BitboardElement& operator|=(const uint64_t other) {
        _data |= other;
        return *this;
    }

    void printBitboard() {
        std::cout << "\n  a b c d e f g h\n";
        for (int rank = 7; rank >= 0; rank--) {
            std::cout << (rank + 1) << " ";
            for (int file = 0; file < 8; file++) {
                int square = rank * 8 + file;
                if (_data & (1ULL << square)) {
                    std::cout << "X ";
                } else {
                    std::cout << ". ";
                }
            }
            std::cout << (rank + 1) << "\n";
            std::cout << std::flush;
        }
        std::cout << "  a b c d e f g h\n";
        std::cout << std::flush;
    }

private:
    uint64_t    _data;

    inline int bitScanForward(uint64_t bb) const {
#if defined(_MSC_VER) && !defined(__clang__)
        unsigned long index;
        _BitScanForward64(&index, bb);
        return index;
#else
        return __builtin_ffsll(bb) - 1;
#endif
    };

};

struct BitMove {
    uint8_t from;
    uint8_t to;
    uint8_t piece;
    uint8_t promotion; // NoPiece (0) = not a promotion

    BitMove(int from, int to, ChessPiece piece, ChessPiece promotion = NoPiece)
        : from(from), to(to), piece(piece), promotion(promotion) { }

    BitMove() : from(0), to(0), piece(NoPiece), promotion(NoPiece) { }

    bool operator==(const BitMove& other) const {
        return from == other.from &&
               to == other.to &&
               piece == other.piece &&
               promotion == other.promotion;
    }
};

// ---------------------------------------------------------------------------
// MoveGen: bitboard-based move generators
// Square index = row * 8 + col  (row 0 = rank 1 = white back rank)
// ---------------------------------------------------------------------------
namespace MoveGen {

constexpr uint64_t kFileA = 0x0101010101010101ULL;
constexpr uint64_t kFileH = 0x8080808080808080ULL;
constexpr uint64_t kRank1 = 0x00000000000000FFULL;
constexpr uint64_t kRank2 = 0x000000000000FF00ULL; // white pawn start
constexpr uint64_t kRank7 = 0x00FF000000000000ULL; // black pawn start
constexpr uint64_t kRank8 = 0xFF00000000000000ULL;

// 12 bitboards: [color 0=white/1=black][ChessPiece value 1-6]
struct BoardState {
    uint64_t pieces[2][7]{}; // index 0 unused; 1=Pawn..6=King
    uint64_t occupied[2]{};
    uint64_t allOccupied{};

    void set(int color, ChessPiece type, int sq) {
        uint64_t bit = 1ULL << sq;
        pieces[color][type] |= bit;
        occupied[color]     |= bit;
        allOccupied         |= bit;
    }
};

// Pop the lowest set bit from bb and return its index.
inline int popLSB(uint64_t& bb) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    bb &= bb - 1;
    return static_cast<int>(idx);
#else
    int idx = __builtin_ctzll(bb);
    bb &= bb - 1;
    return idx;
#endif
}

// Generate all pawn pseudo-legal moves for one color (auto-promotes to queen).
inline void generatePawnMoves(const BoardState& state, int color,
                              std::vector<BitMove>& moves) {
    uint64_t pawns   = state.pieces[color][Pawn];
    uint64_t enemies = state.occupied[1 - color];
    uint64_t empty   = ~state.allOccupied;

    if (color == 0) { // white: advances toward rank 8 (left-shift)
        // --- single push ---
        uint64_t sp = (pawns << 8) & empty;
        uint64_t tmp = sp & ~kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 8,  to, Pawn); }
        tmp = sp & kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 8,  to, Pawn, Queen); }

        // --- double push from rank 2 ---
        tmp = ((pawns & kRank2) << 8 & empty) << 8 & empty;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 16, to, Pawn); }

        // --- NE captures (+9, exclude file H) ---
        uint64_t ne = (pawns & ~kFileH) << 9 & enemies;
        tmp = ne & ~kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 9,  to, Pawn); }
        tmp = ne & kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 9,  to, Pawn, Queen); }

        // --- NW captures (+7, exclude file A) ---
        uint64_t nw = (pawns & ~kFileA) << 7 & enemies;
        tmp = nw & ~kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 7,  to, Pawn); }
        tmp = nw & kRank8;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to - 7,  to, Pawn, Queen); }

    } else { // black: advances toward rank 1 (right-shift)
        // --- single push ---
        uint64_t sp = (pawns >> 8) & empty;
        uint64_t tmp = sp & ~kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 8,  to, Pawn); }
        tmp = sp & kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 8,  to, Pawn, Queen); }

        // --- double push from rank 7 ---
        tmp = ((pawns & kRank7) >> 8 & empty) >> 8 & empty;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 16, to, Pawn); }

        // --- SE captures (-7, exclude file H) ---
        uint64_t se = (pawns & ~kFileH) >> 7 & enemies;
        tmp = se & ~kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 7,  to, Pawn); }
        tmp = se & kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 7,  to, Pawn, Queen); }

        // --- SW captures (-9, exclude file A) ---
        uint64_t sw = (pawns & ~kFileA) >> 9 & enemies;
        tmp = sw & ~kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 9,  to, Pawn); }
        tmp = sw & kRank1;
        while (tmp) { int to = popLSB(tmp); moves.emplace_back(to + 9,  to, Pawn, Queen); }
    }
}

// ---------------------------------------------------------------------------
// Knight move generation (jumps, no path-clearing needed)
// ---------------------------------------------------------------------------
inline void generateKnightMoves(const BoardState& state, int color,
                                std::vector<BitMove>& moves) {
    // Two-file masks prevent wrap when a knight jumps 2 squares sideways
    constexpr uint64_t kFileAB = 0x0303030303030303ULL; // files A+B
    constexpr uint64_t kFileGH = 0xC0C0C0C0C0C0C0C0ULL; // files G+H

    uint64_t knights  = state.pieces[color][Knight];
    uint64_t friendly = state.occupied[color];

    while (knights) {
        int from = popLSB(knights);
        uint64_t sq = 1ULL << from;

        // All 8 L-shaped destinations with correct file-wrap guards
        uint64_t attacks =
            ((sq & ~kFileA)  << 15) | // up-2,   left-1
            ((sq & ~kFileH)  << 17) | // up-2,   right-1
            ((sq & ~kFileAB) << 6)  | // up-1,   left-2
            ((sq & ~kFileGH) << 10) | // up-1,   right-2
            ((sq & ~kFileH)  >> 15) | // down-2, right-1
            ((sq & ~kFileA)  >> 17) | // down-2, left-1
            ((sq & ~kFileGH) >> 6)  | // down-1, right-2
            ((sq & ~kFileAB) >> 10);  // down-1, left-2

        attacks &= ~friendly;

        while (attacks) {
            int to = popLSB(attacks);
            moves.emplace_back(from, to, Knight);
        }
    }
}

// ---------------------------------------------------------------------------
// King move generation (one step in any direction, no castling)
// ---------------------------------------------------------------------------
inline void generateKingMoves(const BoardState& state, int color,
                              std::vector<BitMove>& moves) {
    uint64_t kings    = state.pieces[color][King];
    uint64_t friendly = state.occupied[color];

    while (kings) {
        int from = popLSB(kings);
        uint64_t sq = 1ULL << from;

        uint64_t attacks =
            ((sq & ~kFileA) >> 1)        | // W
            ((sq & ~kFileH) << 1)        | // E
            (sq             << 8)        | // N
            (sq             >> 8)        | // S
            ((sq & ~kFileA) << 7)        | // NW
            ((sq & ~kFileH) << 9)        | // NE
            ((sq & ~kFileA) >> 9)        | // SW
            ((sq & ~kFileH) >> 7);         // SE

        attacks &= ~friendly;

        while (attacks) {
            int to = popLSB(attacks);
            moves.emplace_back(from, to, King);
        }
    }
}

// ---------------------------------------------------------------------------
// Sliding piece generation (rook, bishop, queen) — O(1) magic bitboard lookup.
// No loops or branches per piece: 3 operations → instant attack set.
// Queen = rook attacks | bishop attacks.
// ---------------------------------------------------------------------------
inline void generateSlidingMoves(const BoardState& state, int color,
                                 ChessPiece piece, std::vector<BitMove>& moves) {
    uint64_t pieces   = state.pieces[color][piece];
    uint64_t friendly = state.occupied[color];
    uint64_t occ      = state.allOccupied;

    while (pieces) {
        int from = popLSB(pieces);

        uint64_t attacks;
        if (piece == Rook) {
            attacks = getRookAttacks(from, occ);
        } else if (piece == Bishop) {
            attacks = getBishopAttacks(from, occ);
        } else { // Queen = rook | bishop
            attacks = getRookAttacks(from, occ) | getBishopAttacks(from, occ);
        }

        attacks &= ~friendly; // exclude squares occupied by own pieces

        while (attacks) {
            int to = popLSB(attacks);
            moves.emplace_back(from, to, piece);
        }
    }
}

// ---------------------------------------------------------------------------
// Generate all pseudo-legal moves for `color` using the BoardState.
// This combines pawns, knights, kings and sliding pieces.
// ---------------------------------------------------------------------------
inline void generateAllPseudoLegalMoves(const BoardState& state, int color,
                                       std::vector<BitMove>& moves) {
    generatePawnMoves(state, color, moves);
    generateKnightMoves(state, color, moves);
    generateKingMoves(state, color, moves);
    // bishops, rooks, queens
    generateSlidingMoves(state, color, Bishop, moves);
    generateSlidingMoves(state, color, Rook, moves);
    generateSlidingMoves(state, color, Queen, moves);
}

} // namespace MoveGen