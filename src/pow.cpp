// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "util.h"
#include "primitives/block.h"
#include "uint256.h"
#include "verification_client.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexFirst, pindexLast, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexFirst, const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // TODO: Before PoUW deployment, revert this calculation to the original one of nActualTimeSpan = pindexLast->time - pindexFirst->time
    // If it's not reverted, PAI won't work because the whole PAI blockchain prior to deployment of this change will be seen as invalid
    int64_t nActualTimespan = 0;
    const CBlockIndex* pCur = pindexLast;
    const CBlockIndex* pPrev = pCur->pprev;
    while (pPrev != pindexFirst && pPrev != nullptr)
    {
        int64_t blockTimespan = pCur->GetBlockTime() - pPrev->GetBlockTime();
        if (blockTimespan > params.nPowTargetSpacing * 4)
            blockTimespan = params.nPowTargetSpacing;
        nActualTimespan += blockTimespan;
        pPrev = pPrev->pprev;
        pCur = pCur->pprev;
    }
    assert(pPrev);
    if (pPrev == nullptr)
        return pindexLast->nBits;

    int64_t nExpectedTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    if (nActualTimespan != nExpectedTimespan) {
        int64_t discarded = (nExpectedTimespan - nActualTimespan) / 60;
        LogPrintf("Difficulty retargetting: some block time(s) were too big, possibly due to a pause in mining; we exclude %lld minutes from difficulty calculation.\n", discarded);
    }

    // Limit adjustment step
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);

    arith_uint256 bnMaxCanMultiply = ~arith_uint256() / nActualTimespan;
    if (bnNew > bnMaxCanMultiply)
    {
        // perform division firstly to avoid an overflow
        bnNew /= params.nPowTargetTimespan;
        if (bnNew > bnMaxCanMultiply)
        {
            // still cannot multiply - use pow limit
            bnNew = bnPowLimit;
        }
        else
        {
            bnNew *= nActualTimespan;
        }
    }
    else
    {
        bnNew *= nActualTimespan;
        bnNew /= params.nPowTargetTimespan;
    }

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params& params, bool checkMLproof)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(block.GetHash()) > bnTarget)
        return false;

    // check that the nonce is derived from ML data
    if (block.nNonce != block.DeriveNonceFromML())
        return false;

    // if we are asked only to check block hash against difficulty, we are done
    if (!checkMLproof)
        return true;

    // for genesis block we don't verify ML proof because it's just random data rather than a product of ML training
    if (block.GetHash() == params.hashGenesisBlock)
        return true;

    // in regtest mode, we don't verify ML proof because it's just random data rather than a product of ML training
    if (gArgs.GetBoolArg("-regtest", false))
        return true;

    // if ML proof verification is switched off with a flag, we skip it
    if (gArgs.GetBoolArg("-skipmlcheck", false))
        return true;

    // check ML proof
    std::string verificationServerAddress = gArgs.GetArg("-verificationserver", "localhost:50051");
    VerificationClient client(grpc::CreateChannel(verificationServerAddress, grpc::InsecureChannelCredentials()));
    auto result = client.Verify(std::string(block.powMsgID), block.powModelHash.ToString(), std::string(block.powNextMsgID));
    int resultCode = int(result.first);
    return resultCode == pai::pouw::verification::Response::OK;
}
