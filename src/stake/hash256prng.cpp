#include <algorithm>
#include "stake/hash256prng.h"
#include "stake/hasher.h"


// SEED_CONST is a constant derived from the hex representation of pi.
// It is used along with a caller-provided seed when initializing the deterministic lottery prng.
static const uint8_t SEED_CONST[8] = { 0x24, 0x3F, 0x6A, 0x88, 0x85, 0xA3, 0x08, 0xD3 };

uint256 CalcHash256PRNGIV(const std::vector<char>& seed)
{
    return Hasher().Write(seed.data(), seed.size()).Write(SEED_CONST, sizeof(SEED_CONST)).Finalize();
}

template<typename T, typename V> inline bool exists(const T& c, const V v)
{
    auto end(c.end());
    return end != std::find(c.begin(), end, v);
}

// The CalcHash256PRNGIV can be used to calculate an initialization vector for a given seed such that the generator will produce
// the same values as if Hash256PRNG were called with the same seed.
// This allows callers to cache and reuse the initialization vector for a given seed to avoid recomputation.
Hash256PRNG::Hash256PRNG(const std::vector<char>& seed) : Hash256PRNG(CalcHash256PRNGIV(seed))
{
}

Hash256PRNG::Hash256PRNG(const uint256& iv) :
    seed_(iv),
    hashIdx_(0),
    idx_(0),
    lastHash_(iv)
{
}

uint32_t Hash256PRNG::Hash256Rand()
{
    auto hash = reinterpret_cast<uint32_t*>(lastHash_.begin());
    uint32_t rand = bswap_32(hash[hashIdx_]);
    ++hashIdx_;

    // 'roll over' the hash index to use and store it.
    if (hashIdx_ > 7)
    {
        lastHash_ = Hasher().Write(seed_).Write(bswap_32(static_cast<uint32_t>(idx_))).Finalize();
        ++idx_;
        hashIdx_ = 0;
    }

    // 'roll over' the PRNG by re-hashing the seed when we overflow idx.
    if (idx_ > 0xFFFFFFFF)
    {
        seed_ = Hasher().Write(seed_).Finalize();
        lastHash_ = seed_;
        idx_ = 0;
    }
    return rand;
}

uint256 Hash256PRNG::StateHash()
{
    return Hasher().Write(lastHash_).Write(bswap_32(static_cast<uint32_t>(idx_))).Write(static_cast<uint8_t>(hashIdx_)).Finalize();
}

prevector<64, uint32_t> Hash256PRNG::FindTicketIdxs(uint32_t size, uint32_t n)
{
    if (size < n)
        throw std::runtime_error(std::string(__func__) + ": List size too small");

    prevector<64, uint32_t> list;
    while (list.size() < n)
    {
        uint32_t rand = UniformRandom(size);
        if (!exists(list, rand))
            list.push_back(rand);
    }
    return list;
}

uint32_t Hash256PRNG::UniformRandom(uint32_t upperBound)
{
    if (upperBound < 2)
        return 0;

    uint32_t min;
    if (upperBound > 0x80000000)
        min = 1 + ~upperBound;
    else
        min = ((0xFFFFFFFF - (upperBound * 2)) + 1) % upperBound; // (2**32 - (x * 2)) % x == 2**32 % x when x <= 2**31

    uint32_t rand;
    do
    {
        rand = Hash256Rand();
    } while (rand < min);
    return rand % upperBound;
}
