// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2016 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AUXPOW_H
#define BITCOIN_AUXPOW_H

#include "consensus/params.h"
#include "primitives/pureheader.h"
#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"

#include <vector>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CValidationState;

/** Header for merge-mining data in the coinbase.  */
static const unsigned char pchMergedMiningHeader[] = { 0xfa, 0xbe, 'm', 'm' };

/* Because it is needed for auxpow, the definition of CMerkleTx is moved
   here from wallet.h.  */

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    CTransactionRef tx;
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    /** Helper conversion operator to allow passing CMerkleTx where CTransaction is expected.
    *  TODO: adapt callers and remove this operator. */
    operator const CTransaction&() const { return *tx; }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlockIndex* pindex, int posInBlock);

    /**
     * Actually compute the Merkle branch.  This is used for unit tests when
     * constructing an auxpow.  It is not needed for actual production, since
     * we do not care in the Namecoin client how the auxpow is constructed
     * by a miner.
     */
    void InitMerkleBranch(const CBlock& block, int posInBlock);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    const uint256& GetHash() const { return tx->GetHash(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }

    bool AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state);
};

/**
 * Data for the merge-mining auxpow. This includes a merkle tx (the parent block's
 * coinbase tx) that can be verified to be in the parent block, and this
 * transaction's input (the coinbase script) contains the reference
 * to the actual merge-mined block.
 */
class CAuxPow
{

/* Public for the unit tests.  */
public:
  CMerkleTx* pParentCoinbase;

  /** The merkle branch that proves that parent block references child block.  */
  std::vector<uint256> vChainMerkleBranch;

  /** A companion to the merkle branch, this describes position of child block hash in the merkle tree.  */
  int nChainIndex;

  /** Parent block header (on which the real PoW is done).  */
  CPureBlockHeader parentBlock;

public:

  /* Prevent accidental conversion.  */
  inline explicit CAuxPow (CTransactionRef txIn)
  {
      pParentCoinbase = new CMerkleTx(txIn);
  }

  inline CAuxPow ()
  {
      pParentCoinbase = nullptr;
  }

  inline CAuxPow (const CAuxPow &obj)
  {
      *this = obj;
  }

  inline ~CAuxPow ()
  {
      delete pParentCoinbase;
  }

  CAuxPow& operator=(const CAuxPow& obj)
  {
      vChainMerkleBranch = obj.vChainMerkleBranch;
      nChainIndex = obj.nChainIndex;
      parentBlock = obj.parentBlock;

      pParentCoinbase = nullptr;
      if (obj.pParentCoinbase) {
          pParentCoinbase = new CMerkleTx;
          *pParentCoinbase = *obj.pParentCoinbase;
      }

      return *this;
  }

  ADD_SERIALIZE_METHODS;

  template<typename Stream, typename Operation>
    inline void
    SerializationOp (Stream& s, Operation ser_action)
  {
    if (ser_action.ForRead()) {
        // see if parent coinbase is present
        int parentCoinbaseVersion = 0;
        READWRITE(parentCoinbaseVersion);
        // read parent coinbase if present
        if (parentCoinbaseVersion == 0) // zero is reserved for "not present"
            pParentCoinbase = nullptr;
        else {
            delete pParentCoinbase;
            pParentCoinbase = new CMerkleTx;
            assert(pParentCoinbase);
            READWRITE(*pParentCoinbase);
        }
    }
    else {
        // write coinbase presence
        int parentCoinbaseVersion = pParentCoinbase ? 1 : 0;
        READWRITE(parentCoinbaseVersion);
        // write coinbase if exists
        if (parentCoinbaseVersion != 0)
            READWRITE(*pParentCoinbase);
    }

    READWRITE (vChainMerkleBranch);
    READWRITE (nChainIndex);
    READWRITE (parentBlock);
  }

  /**
   * Verify auxpow.
   * Note that this does not verify the actual PoW on the parent block!  It
   * just confirms that all the merkle branches are valid.
   * @param hashAuxBlock Hash of the merge-mined block.
   * @param params Consensus parameters.
   * @return True if the auxpow is valid.
   */
  bool check (const uint256& hashAuxBlock, const Consensus::Params& params) const;

  /**
   * Get the parent block's hash.  This is used to verify that it
   * satisfies the PoW requirement.
   * @return The parent block hash.
   */
  inline uint256
  getParentBlockHash () const
  {
    return parentBlock.GetHash ();
  }

  /**
   * Check a merkle branch.  This used to be in CBlock, but was removed
   * upstream.  Thus include it here now.
   */
  static uint256 CheckMerkleBranch (uint256 hash,
                                    const std::vector<uint256>& vMerkleBranch,
                                    int nIndex);

  /**
   * Initialise the auxpow of the given block header.  This constructs
   * a minimal CAuxPow object with a minimal parent block and sets
   * it on the block header.  The auxpow is not necessarily valid, but
   * can be "mined" to make it valid.
   * @param header The header to set the auxpow on.
   */
  static void initAuxPow (CBlockHeader& header);

};

#endif // BITCOIN_AUXPOW_H
