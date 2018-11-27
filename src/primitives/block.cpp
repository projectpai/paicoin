// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint32_t CBlockHeader::DeriveNonceFromML() const
{
    unsigned char hashOutput[CHash256::OUTPUT_SIZE];  // a buffer that will hold a temporary hash in native form (32 bytes)
    uint256 hash256;                                  // directly corresponding to the above buffer
    std::string hashConcat;                           // a concatenation of all 3 ML field hashes in hex string form
    CHash256().Write((const unsigned char*)powMsgID, strlen(powMsgID)).Finalize(hashOutput); // hash msg ID
    memcpy(hash256.begin(), hashOutput, CHash256::OUTPUT_SIZE);  // convert msg ID hash to uint256
    hashConcat += hash256.ToString();                            // convert msg ID hash to hex string and concatenate
    hashConcat += powModelHash.ToString();                       // convert model hash to hex string and concatenate
    CHash256().Write((const unsigned char*)powNextMsgID, strlen(powNextMsgID)).Finalize(hashOutput); // hash next msg ID
    memcpy(hash256.begin(), hashOutput, CHash256::OUTPUT_SIZE);  // convert next msg ID hash to uint256
    hashConcat += hash256.ToString();                            // convert next msg ID hash to hex string and concatenate
    CHash256().Write((const unsigned char*)hashConcat.c_str(), hashConcat.length()).Finalize(hashOutput); // hash the concatenated hex string of hashes
    memcpy(hash256.begin(), hashOutput, CHash256::OUTPUT_SIZE);  // convert the final hash to uint256 (note that it is big endian)
    uint64_t hash64 = hash256.GetUint64(0);   // take the first 8 bytes from the final hash (it is now little endian)
    uint32_t hash32 = uint32_t(hash64 & 0x00000000FFFFFFFF);     // take the least significant 4 bytes from the final hash; these correspond to the first 4 bytes of hash256
    return hash32;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, powMsgID=%s, powNextMsgID=%s, powModelHash=%s, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        powMsgID, powNextMsgID, powModelHash.ToString(),
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
