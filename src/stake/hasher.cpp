//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//

#include "stake/hasher.h"


Hasher::Hasher()
{
    Init();
}

void Hasher::Init()
{
    shake256_init(&sha3_ctx_);
}

Hasher& Hasher::Write(uint64_t data)
{
    shake_update(&sha3_ctx_, &data, sizeof(data));
    return *this;
}

Hasher& Hasher::Write(uint32_t data)
{
    shake_update(&sha3_ctx_, &data, sizeof(data));
    return *this;
}

Hasher& Hasher::Write(uint8_t data)
{
    shake_update(&sha3_ctx_, &data, sizeof(data));
    return *this;
}

Hasher& Hasher::Write(const void* data, size_t bytes)
{
    shake_update(&sha3_ctx_, data, bytes);
    return *this;
}

Hasher& Hasher::Write(const uint256& data)
{
    shake_update(&sha3_ctx_, data.begin(), data.size());
    return *this;
}

uint256 Hasher::Finalize()
{
    uint256 result;
    shake_xof(&sha3_ctx_);
    shake_out(&sha3_ctx_, result.begin(), result.size());
    return result;
}
