// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/pureheader.h"
#include "chainparams.h"

#include "hash.h"
#include "utilstrencodings.h"
#include "sync.h"

bool CPureBlockHeader::_serializationForHashCompatibility = false;

static CCriticalSection csSerializationForHashCompatibility;

uint256 CPureBlockHeader::GetHash() const
{
    LOCK(csSerializationForHashCompatibility);
    _serializationForHashCompatibility = true;
    uint256 result = SerializeHash(*this);
    _serializationForHashCompatibility = false;
    return result;
}

bool CPureBlockHeader::SupportsAuxpow() const
{
    uint32_t nActivationTime = GetActivationTime(Consensus::DEPLOYMENT_AUXPOW);
    return nActivationTime > 0 && nTime > nActivationTime;
}
