// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "stake/staketx.h"
#include "crypto/common.h"

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash<CHashWriter>(*this);
    // TODO: use CBlockHashWriter as template argument and regenerate the GENESIS BLOCK
    // return SerializeHash<CBlockHashWriter>(*this);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, "
                   "nStakeDifficulty=%s, nVoteBits=%08x, nTicketPoolSize=%u, ticketLotteryState=%s,"
                   "nFreshStake=%u, nStakeVersion=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        std::to_string(nStakeDifficulty), nVoteBits, nTicketPoolSize, StakeStateToString(ticketLotteryState),
        nFreshStake,  nStakeVersion, vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
