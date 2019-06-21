#ifndef PAICOIN_STAKE_HASH256PRNG_H
#define PAICOIN_STAKE_HASH256PRNG_H

#include "prevector.h"
#include "uint256.h"


// CalcHash256PRNGIV calculates and returns the initialization vector for a given seed.
uint256 CalcHash256PRNGIV(const std::vector<char>& seed);

class Hash256PRNG final
{
public:
    // Creates a deterministic pseudorandom number generator
    // that uses a 256-bit secure hashing function to generate random uint32s given
    // an initialization vector.
    explicit Hash256PRNG(const uint256& iv);

    // Creates a deterministic pseudorandom number generator that
    // uses a 256-bit secure hashing function to generate random uint32s given a
    // seed.
    explicit Hash256PRNG(const std::vector<char>& seed);

public:
    // Returns a uint32 random number using the pseudorandom number generator and updates the state.
    uint32_t Hash256Rand();

    // Returns a hash referencing the current state the deterministic PRNG.
    uint256 StateHash();

    // Finds n many unique index numbers for a list length size.
    prevector<64, uint32_t> FindTicketIdxs(uint32_t size, uint32_t n);

private:
    // Returns a random in the range [0 ... upperBound) while avoiding
    // modulo bias, thus giving a normal distribution within the specified range.
    uint32_t UniformRandom(uint32_t upperBound);

private:
    uint256  seed_;     // The seed used to initialize
    uint32_t hashIdx_;  // Position in the cached hash
    uint64_t idx_;      // Position in the hash iterator
    uint256  lastHash_; // Cached last hash used
};

#endif // PAICOIN_STAKE_HASH256PRNG_H
