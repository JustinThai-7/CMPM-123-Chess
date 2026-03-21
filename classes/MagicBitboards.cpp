#include "MagicBitboards.h"
#include <cstdlib>
#include <vector>

// Table storage (defined here to avoid ODR and keep large arrays out of headers)
uint64_t RMasks[64];
uint64_t BMasks[64];
uint64_t RMagic[64];
uint64_t BMagic[64];
int      RShifts[64];
int      BShifts[64];
uint64_t RAttacks[64][4096];
uint64_t BAttacks[64][512];

// Build a 64-bit random value from multiple rand() calls.
static uint64_t rand64() {
    return  (uint64_t)(rand() & 0x7FFF)
          | ((uint64_t)(rand() & 0x7FFF) << 15)
          | ((uint64_t)(rand() & 0x7FFF) << 30)
          | ((uint64_t)(rand() & 0x7FFF) << 45)
          | ((uint64_t)(rand() & 0xF)    << 60);
}

// Sparse random (triple-AND) to produce low-density 1-bits for magic search.
uint64_t getRandomUint64FewBits() { return rand64() & rand64() & rand64(); }

// Rook occupancy mask: interior squares along rank and file (exclude edges).
static uint64_t rookMask(int sq) {
    uint64_t mask = 0;
    int rank = sq / 8, file = sq % 8;
    for (int f = 1; f < 7; f++)
        if (f != file) mask |= 1ULL << (rank * 8 + f);
    for (int r = 1; r < 7; r++)
        if (r != rank) mask |= 1ULL << (r * 8 + file);
    return mask;
}

// Bishop occupancy mask: interior diagonal squares (exclude edges).
static uint64_t bishopMask(int sq) {
    uint64_t mask = 0;
    int rank = sq / 8, file = sq % 8;
    for (int r = rank+1, f = file+1; r <= 6 && f <= 6; r++, f++) mask |= 1ULL << (r*8+f);
    for (int r = rank+1, f = file-1; r <= 6 && f >= 1; r++, f--) mask |= 1ULL << (r*8+f);
    for (int r = rank-1, f = file+1; r >= 1 && f <= 6; r--, f++) mask |= 1ULL << (r*8+f);
    for (int r = rank-1, f = file-1; r >= 1 && f >= 1; r--, f--) mask |= 1ULL << (r*8+f);
    return mask;
}

// Slow rook attack generator used only during table construction.
static uint64_t rookAttacksOTF(int sq, uint64_t occ) {
    uint64_t attacks = 0;
    int rank = sq / 8, file = sq % 8;
    for (int r = rank+1; r < 8; r++) { uint64_t b = 1ULL << (r*8+file); attacks |= b; if (occ & b) break; }
    for (int r = rank-1; r >= 0; r--) { uint64_t b = 1ULL << (r*8+file); attacks |= b; if (occ & b) break; }
    for (int f = file+1; f < 8; f++) { uint64_t b = 1ULL << (rank*8+f);  attacks |= b; if (occ & b) break; }
    for (int f = file-1; f >= 0; f--) { uint64_t b = 1ULL << (rank*8+f); attacks |= b; if (occ & b) break; }
    return attacks;
}

// Slow bishop attack generator used only during table construction.
static uint64_t bishopAttacksOTF(int sq, uint64_t occ) {
    uint64_t attacks = 0;
    int rank = sq / 8, file = sq % 8;
    for (int r = rank+1, f = file+1; r < 8 && f < 8; r++, f++) { uint64_t b = 1ULL << (r*8+f); attacks |= b; if (occ & b) break; }
    for (int r = rank+1, f = file-1; r < 8 && f >= 0; r++, f--) { uint64_t b = 1ULL << (r*8+f); attacks |= b; if (occ & b) break; }
    for (int r = rank-1, f = file+1; r >= 0 && f < 8; r--, f++) { uint64_t b = 1ULL << (r*8+f); attacks |= b; if (occ & b) break; }
    for (int r = rank-1, f = file-1; r >= 0 && f >= 0; r--, f--) { uint64_t b = 1ULL << (r*8+f); attacks |= b; if (occ & b) break; }
    return attacks;
}

// FindMagic: search for a collision-free magic multiplier for `sq`.
// Uses subset enumeration, a popcount heuristic, and sparse random trials.
uint64_t findMagic(int sq, int bits, bool isBishop) {
    uint64_t mask = isBishop ? BMasks[sq] : RMasks[sq];
    int n = 1 << bits;

    std::vector<uint64_t> blockers(n), attacks(n);

    // Carry-Rippler: enumerate every subset of `mask` in order
    uint64_t subset = 0;
    for (int i = 0; i < n; i++) {
        blockers[i] = subset;
        attacks[i]  = isBishop ? bishopAttacksOTF(sq, subset) : rookAttacksOTF(sq, subset);
        subset = (subset - mask) & mask;
    }

    // Pre-allocate outside the try-loop: avoids heap allocation on every attempt.
    // epoch[idx] records which try last wrote slot idx, so we never need to memset.
    std::vector<uint64_t> used(n, 0ULL);
    std::vector<int>      epoch(n, -1);

    for (int tries = 0; tries < 1000000; tries++) {
        uint64_t magic = getRandomUint64FewBits();

        // Heuristic from spec: (mask * magic) >> 56 must have ≥6 bits set.
        // Filters obviously bad candidates before the expensive loop below.
        if (__builtin_popcountll((mask * magic) >> 56) < 6) continue;

        bool failed = false;

        for (int i = 0; i < n && !failed; i++) {
            int idx = (int)((blockers[i] * magic) >> (64 - bits));
            if (epoch[idx] != tries) {
                // Slot not yet written this attempt — claim it.
                epoch[idx] = tries;
                used[idx]  = attacks[i];
            } else if (used[idx] != attacks[i]) {
                // Collision: different attack sets mapped to same index.
                failed = true;
            }
        }

        if (!failed) return magic;
    }

    return 0ULL; // unreachable in practice
}

void initMagics() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    srand(1234);

    for (int sq = 0; sq < 64; sq++) {
        uint64_t subset = 0;
        // --- Rook ---
        RMasks[sq]  = rookMask(sq);
        int rbits   = __builtin_popcountll(RMasks[sq]);
        if (rbits == 0) {
            // No relevant occupancy bits: single-table entry (index 0).
            RShifts[sq] = 0;
            RMagic[sq]  = 1ULL;
            RAttacks[sq][0] = rookAttacksOTF(sq, 0ULL);
        } else {
            RShifts[sq] = 64 - rbits;
            RMagic[sq]  = findMagic(sq, rbits, false);

            uint64_t subset = 0;
            do {
                int idx = (int)((subset * RMagic[sq]) >> RShifts[sq]);
                RAttacks[sq][idx] = rookAttacksOTF(sq, subset);
                subset = (subset - RMasks[sq]) & RMasks[sq];
            } while (subset != 0);
        }

        // --- Bishop ---
        BMasks[sq]  = bishopMask(sq);
        int bbits   = __builtin_popcountll(BMasks[sq]);
        if (bbits == 0) {
            BShifts[sq] = 0;
            BMagic[sq]  = 1ULL;
            BAttacks[sq][0] = bishopAttacksOTF(sq, 0ULL);
        } else {
            BShifts[sq] = 64 - bbits;
            BMagic[sq]  = findMagic(sq, bbits, true);

            subset = 0;
            do {
                int idx = (int)((subset * BMagic[sq]) >> BShifts[sq]);
                BAttacks[sq][idx] = bishopAttacksOTF(sq, subset);
                subset = (subset - BMasks[sq]) & BMasks[sq];
            } while (subset != 0);
        }
    }
}
