// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_PUREHEADER_H
#define BITCOIN_PRIMITIVES_PUREHEADER_H

#include "serialize.h"
#include "uint256.h"

#include <boost/shared_ptr.hpp>

/**
 * A block header without auxpow information.  This "intermediate step"
 * in constructing the full header is useful, because it breaks the cyclic
 * dependency between auxpow (referencing a parent block header) and
 * the block header (referencing an auxpow).  The parent block header
 * does not have auxpow itself, so it is a pure header.
 */
class CPureBlockHeader
{
    static bool _serializationForHashCompatibility;

public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    boost::shared_ptr<uint256> pAuxpowChildrenHash;

    CPureBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        // block header hash is calculated using serialization and hashing the serialized stream (see GetHash() member);
        // in case "auxpow children hash" is not present, we must not let null pointer contribute to header hash;
        // without this provision, it wouldn't be possible to use Bitcoin block header as auxpow,
        // as it would hash to something else than it did in Bitcoin, and thus no longer satisfy difficulty
        if (pAuxpowChildrenHash == nullptr && _serializationForHashCompatibility)
            return;

        bool ParamsInitialized();

        if (ParamsInitialized() && this->SupportsAuxpow()) // chainParams aren't initialized only during genesis block serialization
        {
            if (ser_action.ForRead()) {
                // see if merged mining children hash is present
                int childrenHashVersion = 0;
                READWRITE(childrenHashVersion);
                // read children hash if present
                if (childrenHashVersion == 0)   // zero is reserved for "not present"
                    pAuxpowChildrenHash.reset();
                else {
                    pAuxpowChildrenHash.reset(new uint256);
                    assert(pAuxpowChildrenHash);
                    READWRITE(*pAuxpowChildrenHash);
                }
            }
            else {
                // write children hash presence
                int childrenHashVersion = pAuxpowChildrenHash ? 1 : 0;
                READWRITE(childrenHashVersion);
                // write children hash if exists
                if (childrenHashVersion != 0)
                    READWRITE(*pAuxpowChildrenHash);
            }
        }
        else if (ser_action.ForRead())
            pAuxpowChildrenHash.reset();
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        pAuxpowChildrenHash.reset();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    friend bool operator==(const CPureBlockHeader& a, const CPureBlockHeader& b)
    {
        return a.nVersion == b.nVersion &&
               a.hashPrevBlock == b.hashPrevBlock &&
               a.hashMerkleRoot == b.hashMerkleRoot &&
               a.nTime == b.nTime &&
               a.nBits == b.nBits &&
               a.nNonce == b.nNonce;
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    /**
     * Is the block created after auxpow is activated?
     * Blocks that do not support auxpow must not have auxpow (they could not have been merge-mined).
     * Blocks that do support auxpow may or may not have auxpow (depending if they have been merge-mined or mined directly).
     * @return True iff this block is newer than auxpow fork activation date.
     */
    bool SupportsAuxpow() const;
};

#endif // BITCOIN_PRIMITIVES_PUREHEADER_H
