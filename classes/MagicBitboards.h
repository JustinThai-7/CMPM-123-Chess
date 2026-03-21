#pragma once
#include <cstdint>

// Magic bitboard tables and accessors. Call initMagics() once at startup.

extern uint64_t RMasks[64];      // rook occupancy masks
extern uint64_t BMasks[64];      // bishop occupancy masks
extern uint64_t RMagic[64];      // rook magic multipliers 
extern uint64_t BMagic[64];      // bishop magic multipliers
extern int      RShifts[64];     // rook right-shift amounts
extern int      BShifts[64];     // bishop right-shift amounts
extern uint64_t RAttacks[64][4096]; // rook attack lookup table
extern uint64_t BAttacks[64][512];  // bishop attack lookup table

void     initMagics();
uint64_t getRandomUint64FewBits();
uint64_t findMagic(int sq, int bits, bool isBishop);

inline uint64_t getRookAttacks(int sq, uint64_t occ) {
    occ &= RMasks[sq];
    occ *= RMagic[sq];
    occ >>= RShifts[sq];
    return RAttacks[sq][occ];
}

inline uint64_t getBishopAttacks(int sq, uint64_t occ) {
    occ &= BMasks[sq];
    occ *= BMagic[sq];
    occ >>= BShifts[sq];
    return BAttacks[sq][occ];
}
