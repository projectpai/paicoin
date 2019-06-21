#ifndef PAICOIN_STAKE_HASHER_H
#define PAICOIN_STAKE_HASHER_H

extern "C" {
#include "crypto/tiny_sha3.h"
}
#include "uint256.h"


class Hasher final
{
public:
    Hasher();

    Hasher(const Hasher&) = delete;
    Hasher(Hasher&&) = delete;

    Hasher& operator=(const Hasher&) = delete;
    Hasher& operator=(Hasher&&) = delete;

public:
    void Init();

    Hasher& Write(uint64_t data);

    Hasher& Write(uint32_t data);

    Hasher& Write(uint8_t data);

    Hasher& Write(const void* data, size_t bytes);

    Hasher& Write(const uint256& data);

    uint256 Finalize();

private:
    sha3_ctx_t sha3_ctx_;
};

#endif // PAICOIN_STAKE_HASHER_H
