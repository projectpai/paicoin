// Copyright (c) 2014-2015 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "auxpow.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/merkle.h"
#include "validation.h"
#include "primitives/block.h"
#include "script/script.h"
#include "utilstrencodings.h"
#include "uint256.h"

#include "test/test_paicoin.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

/* No space between BOOST_FIXTURE_TEST_SUITE and '(', so that extraction of
   the test-suite name works with grep as done in the Makefile.  */
BOOST_FIXTURE_TEST_SUITE(auxpow_tests, BasicTestingSetup)

/* ************************************************************************** */

/**
 * Tamper with a uint256 (modify it).
 * @param num The number to modify.
 */
static void
tamperWith (uint256& num)
{
  arith_uint256 modifiable = UintToArith256 (num);
  modifiable += 1;
  num = ArithToUint256 (modifiable);
}

/**
 * Utility class to construct auxpow's and manipulate them.  This is used
 * to simulate various scenarios.
 */
class CAuxpowBuilder
{
public:

  /** The parent block (with coinbase, not just header).  */
  CBlock parentBlock;

  /** The auxpow's merkle branch (connecting it to the coinbase).  */
  std::vector<uint256> auxpowChainMerkleBranch;
  /** The auxpow's merkle tree index.  */
  int auxpowChainIndex;

  /**
   * Initialise everything.
   * @param parentBlockVerion The parent block version to use.
   */
  CAuxpowBuilder (int parentBlockVerion);

  /**
   * Set the coinbase's script.
   * @param scr Set it to this script.
   */
  void setCoinbase (const CScript& scr);

  /**
   * Set the merkle tree root hash in parent block.
   * @param auxRootHash Set it to this hash.
   */
  void setChildrenHash (const uint256& auxRootHash);

  /**
   * Build the auxpow merkle branch.  The member variables will be
   * set accordingly.  This has to be done before constructing the coinbase
   * itself (which must contain the root merkle hash).  When we have the
   * coinbase afterwards, the member variables can be used to initialise
   * the CAuxPow object from it.
   * @param hashAux The merge-mined chain's block hash.
   * @param h Height of the merkle tree to build.
   * @return The root hash, with reversed endian.
   */
  valtype buildAuxpowChain (const uint256& hashAux, unsigned h);

  /**
   * Build the finished CAuxPow object of the classic format (with coinbase).
   * We assume that the auxpowChain member variables are already set.
   * We use the passed in transaction as the base. It should (probably) be the parent block's coinbase.
   * @param tx The base tx to use.
   * @return The constructed CAuxPow object.
   */
  CAuxPow getClassic(const CTransactionRef tx) const;

  /**
   * Build the finished CAuxPow object of the lean format (without coinbase).
   * We assume that the auxpowChain member variables are already set.
   * @return The constructed CAuxPow object.
   */
  CAuxPow getLean() const;

  /**
   * Build the finished CAuxPow object from the parent block's coinbase.
   * @return The constructed CAuxPow object.
   */
  inline CAuxPow get() const
  {
    if (parentBlock.pAuxpowChildrenHash != nullptr)
        return getLean();
    else {
        assert (!parentBlock.vtx.empty ());
        return getClassic(parentBlock.vtx[0]);
    }
  }

  /**
   * Build a data vector to be included in the coinbase.  It consists
   * of the aux hash, the merkle tree size and the nonce.  Optionally,
   * the header can be added as well.
   * @param header Add the header?
   * @param auxRoot The aux merkle root hash.
   * @return The constructed data.
   */
  static valtype buildCoinbaseData (bool header, const valtype& auxRoot);

};

CAuxpowBuilder::CAuxpowBuilder (int parentBlockVersion)
  : auxpowChainIndex(-1)
{
  parentBlock.nVersion = parentBlockVersion;
}

void
CAuxpowBuilder::setCoinbase (const CScript& scr)
{
  CMutableTransaction mtx;
  mtx.vin.resize (1);
  mtx.vin[0].prevout.SetNull ();
  mtx.vin[0].scriptSig = scr;

  parentBlock.vtx.clear ();
  parentBlock.vtx.push_back (MakeTransactionRef (std::move (mtx)));
  parentBlock.hashMerkleRoot = BlockMerkleRoot (parentBlock);
}

void
CAuxpowBuilder::setChildrenHash (const uint256& auxRootHash)
{
    if (parentBlock.pAuxpowChildrenHash != nullptr)
        *parentBlock.pAuxpowChildrenHash = auxRootHash;
    else
        parentBlock.pAuxpowChildrenHash.reset(new uint256(auxRootHash));
}

valtype
CAuxpowBuilder::buildAuxpowChain (const uint256& hashAux, unsigned h)
{
  auxpowChainIndex = 0;

  /* Just use "something" for the branch.  Doesn't really matter.  */
  auxpowChainMerkleBranch.clear ();
  for (unsigned i = 0; i < h; ++i)
    auxpowChainMerkleBranch.push_back (ArithToUint256 (arith_uint256 (i)));

  const uint256 hash
    = CAuxPow::CheckMerkleBranch (hashAux, auxpowChainMerkleBranch, auxpowChainIndex);

  valtype res = ToByteVector (hash);
  std::reverse (res.begin (), res.end ());

  return res;
}

CAuxPow
CAuxpowBuilder::getClassic(const CTransactionRef tx) const
{
  LOCK(cs_main);
  CAuxPow res(tx);
  res.pParentCoinbase->InitMerkleBranch (parentBlock, 0);

  res.vChainMerkleBranch = auxpowChainMerkleBranch;
  res.nChainIndex = auxpowChainIndex;
  res.parentBlock = parentBlock;

  return res;
}

CAuxPow
CAuxpowBuilder::getLean() const
{
  LOCK(cs_main);
  CAuxPow res;
  res.vChainMerkleBranch = auxpowChainMerkleBranch;
  res.nChainIndex = auxpowChainIndex;
  res.parentBlock = parentBlock;

  return res;
}

valtype
CAuxpowBuilder::buildCoinbaseData (bool header, const valtype& auxRoot)
{
  valtype res;

  if (header)
    res.insert (res.end (), UBEGIN (pchMergedMiningHeader),
                UEND (pchMergedMiningHeader));
  res.insert (res.end (), auxRoot.begin (), auxRoot.end ());

  return res;
}

/* ************************************************************************** */

BOOST_AUTO_TEST_CASE (check_auxpow)
{
  // check classic auxpow
  {
  const Consensus::Params& params = Params ().GetConsensus ();
  CAuxpowBuilder builder(5);
  CAuxPow auxpow;

  const uint256 hashAux = ArithToUint256 (arith_uint256(12345));
  const unsigned height = 30;

  valtype auxRoot, data;
  CScript scr;

  /* Build a correct auxpow. */
  auxRoot = builder.buildAuxpowChain (hashAux, height);
  data = CAuxpowBuilder::buildCoinbaseData (true, auxRoot);
  scr = (CScript () << 2809 << 2013) + COINBASE_FLAGS;
  scr = (scr << OP_2 << data);
  builder.setCoinbase (scr);
  BOOST_CHECK (builder.get ().check (hashAux, params));

  /* Check that the auxpow is invalid if we change the aux block's hash.  */
  uint256 modifiedAux(hashAux);
  tamperWith (modifiedAux);
  BOOST_CHECK (!builder.get ().check (modifiedAux, params));

  /* Non-coinbase parent tx should fail.  Note that we can't just copy
     the coinbase literally, as we have to get a tx with different hash.  */
  const CTransactionRef oldCoinbase = builder.parentBlock.vtx[0];
  builder.setCoinbase (scr << 5);
  builder.parentBlock.vtx.push_back (oldCoinbase);
  builder.parentBlock.hashMerkleRoot = BlockMerkleRoot (builder.parentBlock);
  auxpow = builder.getClassic(builder.parentBlock.vtx[0]);
  BOOST_CHECK (auxpow.check (hashAux, params));
  auxpow = builder.getClassic(builder.parentBlock.vtx[1]);
  BOOST_CHECK (!auxpow.check (hashAux, params));

  /* Verify that we compare correctly to the parent block's merkle root.  */
  CAuxpowBuilder builder2 = builder;
  BOOST_CHECK (builder2.get ().check (hashAux, params));
  tamperWith (builder2.parentBlock.hashMerkleRoot);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));

  /* Build a non-header legacy version and check that it is also accepted.  */
  builder2 = builder;
  auxRoot = builder2.buildAuxpowChain (hashAux, height);
  data = CAuxpowBuilder::buildCoinbaseData (false, auxRoot);
  scr = (CScript () << 2809 << 2013) + COINBASE_FLAGS;
  scr = (scr << OP_2 << data);
  builder2.setCoinbase (scr);
  BOOST_CHECK (builder2.get ().check (hashAux, params));

  /* However, various attempts at smuggling two roots in should be detected.  */

  const valtype wrongAuxRoot = builder2.buildAuxpowChain (modifiedAux, height);
  valtype data2 = CAuxpowBuilder::buildCoinbaseData (false, wrongAuxRoot);
  builder2.setCoinbase (CScript () << data << data2);
  BOOST_CHECK (builder2.get ().check (hashAux, params));
  builder2.setCoinbase (CScript () << data2 << data);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));

  data2 = CAuxpowBuilder::buildCoinbaseData (true, wrongAuxRoot);
  builder2.setCoinbase (CScript () << data << data2);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));
  builder2.setCoinbase (CScript () << data2 << data);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));

  data = CAuxpowBuilder::buildCoinbaseData (true, auxRoot);
  builder2.setCoinbase (CScript () << data << data2);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));
  builder2.setCoinbase (CScript () << data2 << data);
  BOOST_CHECK (!builder2.get ().check (hashAux, params));

  data2 = CAuxpowBuilder::buildCoinbaseData (false, wrongAuxRoot);
  builder2.setCoinbase (CScript () << data << data2);
  BOOST_CHECK (builder2.get ().check (hashAux, params));
  builder2.setCoinbase (CScript () << data2 << data);
  BOOST_CHECK (builder2.get ().check (hashAux, params));
  }

  // check lean auxpow
  {
  const Consensus::Params& params = Params ().GetConsensus ();
  CAuxpowBuilder builder(5);
  CAuxPow auxpow;

  const uint256 hashAux = ArithToUint256 (arith_uint256(12345));
  const unsigned height = 30;

  /* Build a correct auxpow. */
  valtype auxRoot = builder.buildAuxpowChain (hashAux, height);
  const uint256 auxRootHash = CAuxPow::CheckMerkleBranch (hashAux, builder.auxpowChainMerkleBranch, builder.auxpowChainIndex);
  builder.setChildrenHash(auxRootHash);
  BOOST_CHECK (builder.get ().check (hashAux, params));

  /* Check that the auxpow is invalid if we change the aux block's hash.  */
  uint256 modifiedAux(hashAux);
  tamperWith (modifiedAux);
  BOOST_CHECK (!builder.get ().check (modifiedAux, params));
  }
}

/* ************************************************************************** */

/**
 * Mine a block (assuming minimal difficulty) that either matches
 * or doesn't match the difficulty target specified in the block header.
 * @param block The block to mine (by updating nonce).
 * @param ok Whether the block should be ok for PoW.
 * @param nBits Use this as difficulty if specified.
 */
static void
mineBlock (CBlockHeader& block, bool ok, int nBits = -1)
{
  if (nBits == -1)
    nBits = block.nBits;

  arith_uint256 target;
  target.SetCompact (nBits);

  block.nNonce = 0;
  while (true)
    {
      const bool nowOk = (UintToArith256 (block.GetHash ()) <= target);
      if ((ok && nowOk) || (!ok && !nowOk))
        break;

      ++block.nNonce;
    }

  if (ok)
    BOOST_CHECK (CheckProofOfWork (block.GetHash (), nBits, Params().GetConsensus()));
  else
    BOOST_CHECK (!CheckProofOfWork (block.GetHash (), nBits, Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE (auxpow_pow)
{
  /* Use regtest parameters to allow mining with easy difficulty.  */
  SelectParams (CBaseChainParams::REGTEST);
  const Consensus::Params& params = Params().GetConsensus();

  /* Verify mining without auxpow */
  const arith_uint256 target = (~arith_uint256(0) >> 1);
  CBlockHeader block;
  block.nBits = target.GetCompact ();
  block.nTime = params.nAuxpowActivationTime + 1;
  mineBlock (block, false);
  BOOST_CHECK (!CheckProofOfWork (block, params));
  mineBlock (block, true);
  BOOST_CHECK (CheckProofOfWork (block, params));

  /* Verify merged mining with classic auxpow.  */
  {
  CAuxpowBuilder builder(5);
  CAuxPow auxpow;
  const unsigned height = 3;
  valtype auxRoot, data;

  /* Valid auxpow, PoW check of parent block.  */
  auxRoot = builder.buildAuxpowChain (block.GetHash (), height);
  data = CAuxpowBuilder::buildCoinbaseData (true, auxRoot);
  builder.setCoinbase (CScript () << data);
  mineBlock (builder.parentBlock, false, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (!CheckProofOfWork (block, params));
  mineBlock (builder.parentBlock, true, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (CheckProofOfWork (block, params));

  /* Modifying the block invalidates the PoW.  */
  auxRoot = builder.buildAuxpowChain (block.GetHash (), height);
  data = CAuxpowBuilder::buildCoinbaseData (true, auxRoot);
  builder.setCoinbase (CScript () << data);
  mineBlock (builder.parentBlock, true, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (CheckProofOfWork (block, params));
  tamperWith (block.hashMerkleRoot);
  BOOST_CHECK (!CheckProofOfWork (block, params));
  }

  /* Verify merged mining with lean auxpow.  */
  {
  CAuxpowBuilder builder(5);
  CAuxPow auxpow;
  const unsigned height = 3;
  valtype auxRoot;

  /* Valid auxpow, PoW check of parent block.  */
  auxRoot = builder.buildAuxpowChain (block.GetHash (), height);
  uint256 auxRootHash = CAuxPow::CheckMerkleBranch (block.GetHash(), builder.auxpowChainMerkleBranch, builder.auxpowChainIndex);
  builder.setChildrenHash(auxRootHash);
  mineBlock (builder.parentBlock, false, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (!CheckProofOfWork (block, params));
  mineBlock (builder.parentBlock, true, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (CheckProofOfWork (block, params));

  /* Modifying the block invalidates the PoW.  */
  auxRoot = builder.buildAuxpowChain (block.GetHash (), height);
  auxRootHash = CAuxPow::CheckMerkleBranch (block.GetHash(), builder.auxpowChainMerkleBranch, builder.auxpowChainIndex);
  builder.setChildrenHash(auxRootHash);
  mineBlock (builder.parentBlock, true, block.nBits);
  block.SetAuxpow (new CAuxPow (builder.get ()));
  BOOST_CHECK (CheckProofOfWork (block, params));
  tamperWith (block.hashMerkleRoot);
  BOOST_CHECK (!CheckProofOfWork (block, params));
  }
}

/* ************************************************************************** */

BOOST_AUTO_TEST_SUITE_END ()
