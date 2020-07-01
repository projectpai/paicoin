// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_stake_test_fixture.h"

#include <boost/test/unit_test.hpp>

#include "validation.h"
#include "wallet/coincontrol.h"
#include "rpc/server.h"

BOOST_FIXTURE_TEST_SUITE(vote_tests, WalletStakeTestingSetup)

// test the stakebase content verifier
BOOST_FIXTURE_TEST_CASE(stakebase_detection, WalletStakeTestingSetup)
{
    const uint256& dummyOutputHash = GetRandHash();
    const uint256& prevOutputHash = emptyData.hash;
    const uint32_t& prevOutputIndex = static_cast<uint32_t>(-1);
    const CScript& stakeBaseSigScript = consensus.stakeBaseSigScript;

    // prevout.n
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 0, stakeBaseSigScript)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 1, stakeBaseSigScript)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 2, stakeBaseSigScript)), false);

    // sigScript
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript())), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01 << 0x02)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01)), false);

    // prevout.hash
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(dummyOutputHash, prevOutputIndex, stakeBaseSigScript)), false);

    // correct
    BOOST_CHECK(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, stakeBaseSigScript)));
}

// test the vote bits
BOOST_FIXTURE_TEST_CASE(vote_bits, WalletStakeTestingSetup)
{
    // default values

    BOOST_CHECK(VoteBits::allRejected.getBits() == 0x0000);
    BOOST_CHECK(VoteBits::rttAccepted.getBits() == 0x0001);

    // default construction

    BOOST_CHECK(VoteBits().getBits() == 0x0000);

    // construction from uint16_t

    BOOST_CHECK(VoteBits(0x0000).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(0x0001).getBits() == 0x0001);
    BOOST_CHECK(VoteBits(0x1234).getBits() == 0x1234);
    BOOST_CHECK(VoteBits(0xFFFF).getBits() == 0xFFFF);

    // construction with signaled bit

    BOOST_CHECK(VoteBits(VoteBits::Rtt, true).getBits() == 0x0001);
    BOOST_CHECK(VoteBits(VoteBits::Rtt, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(1, true).getBits() == 0x0002);
    BOOST_CHECK(VoteBits(1, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(7, true).getBits() == 0x0080);
    BOOST_CHECK(VoteBits(7, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(15, true).getBits() == 0x8000);
    BOOST_CHECK(VoteBits(15, false).getBits() == 0x0000);

    // copy constructor (also testing getting all the bits)

    VoteBits vb1;
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x0000);

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x0001);

    vb1 = VoteBits(0x1234);
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x1234);

    vb1 = VoteBits(0xFFFF);
    BOOST_CHECK(VoteBits(vb1).getBits() == 0xFFFF);

    // move constructor

    vb1 = VoteBits(0x0000);
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x0000);

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x0001);

    vb1 = VoteBits(0x1234);
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x1234);

    vb1 = VoteBits(0xFFFF);
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0xFFFF);

    // copy assignment

    vb1 = VoteBits(0x0000);
    VoteBits vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x0000);

    vb1 = VoteBits(0x0001);
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x0001);

    vb1 = VoteBits(0x1234);
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x1234);

    vb1 = VoteBits(0xFFFF);
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0xFFFF);

    // move assignment

    vb1 = VoteBits(0x0000);
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x0000);

    vb1 = VoteBits(0x0001);
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x0001);

    vb1 = VoteBits(0x1234);
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x1234);

    vb1 = VoteBits(0xFFFF);
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0xFFFF);

    // equality

    vb1 = VoteBits(0x0000);
    BOOST_CHECK(vb1 == VoteBits(0x0000));

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(vb1 == VoteBits(0x0001));

    vb1 = VoteBits(0x1234);
    BOOST_CHECK(vb1 == VoteBits(0x1234));

    vb1 = VoteBits(0xFFFF);
    BOOST_CHECK(vb1 == VoteBits(0xFFFF));

    // inequality

    vb1 = VoteBits(0x0000);
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0x1234));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x1234));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = VoteBits(0x1234);
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = VoteBits(0xFFFF);
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0x1234));

    // set bit

    vb1 = VoteBits(0x0000);

    vb1.setBit(VoteBits::Rtt, true);
    BOOST_CHECK(vb1.getBits() == 0x0001);
    vb1.setBit(VoteBits::Rtt, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(1, true);
    BOOST_CHECK(vb1.getBits() == 0x0002);
    vb1.setBit(1, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(7, true);
    BOOST_CHECK(vb1.getBits() == 0x0080);
    vb1.setBit(7, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(15, true);
    BOOST_CHECK(vb1.getBits() == 0x8000);
    vb1.setBit(15, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    // get bit

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(vb1.getBit(VoteBits::Rtt));
    vb1 = VoteBits(0x0000);
    BOOST_CHECK(!vb1.getBit(VoteBits::Rtt));

    vb1 = VoteBits(0x0002);
    BOOST_CHECK(vb1.getBit(1));
    vb1 = VoteBits(0x0000);
    BOOST_CHECK(!vb1.getBit(1));

    vb1 = VoteBits(0x0080);
    BOOST_CHECK(vb1.getBit(7));
    vb1 = VoteBits(0x0000);
    BOOST_CHECK(!vb1.getBit(7));

    vb1 = VoteBits(0x8000);
    BOOST_CHECK(vb1.getBit(15));
    vb1 = VoteBits(0x0000);
    BOOST_CHECK(!vb1.getBit(15));

    vb1 = VoteBits(0x0003);
    BOOST_CHECK(vb1.getBit(0));
    BOOST_CHECK(vb1.getBit(1));
    BOOST_CHECK(!vb1.getBit(2));

    vb1 = VoteBits(0xFFFE);
    BOOST_CHECK(!vb1.getBit(0));
    BOOST_CHECK(vb1.getBit(1));
    BOOST_CHECK(vb1.getBit(2));
    BOOST_CHECK(vb1.getBit(7));
    BOOST_CHECK(vb1.getBit(15));

    // RTT acceptance verification

    vb1 = VoteBits(0x0001);
    BOOST_CHECK(vb1.isRttAccepted());
    vb1 = VoteBits(0x4321);
    BOOST_CHECK(vb1.isRttAccepted());
    vb1 = VoteBits(0xCDEF);
    BOOST_CHECK(vb1.isRttAccepted());

    vb1 = VoteBits(0x0000);
    BOOST_CHECK(!vb1.isRttAccepted());
    vb1 = VoteBits(0x1234);
    BOOST_CHECK(!vb1.isRttAccepted());
    vb1 = VoteBits(0xFFFE);
    BOOST_CHECK(!vb1.isRttAccepted());

    // RTT acceptance control

    vb1 = VoteBits(0x0000);
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0x0001);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1 = VoteBits(0x1234);
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0x1235);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0x1234);

    vb1 = VoteBits(0xFFFE);
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0xFFFF);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0xFFFE);

    vb1 = VoteBits(0xFFFF);
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0xFFFF);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0xFFFE);
}

// test the extended vote bits functions
BOOST_FIXTURE_TEST_CASE(extended_vote_bits, WalletStakeTestingSetup)
{
    // string validation

    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("1234567890abcdef"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(extendedVoteBitsData.validLargeString));

    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("0"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("1"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("012"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("12345"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("abc"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("lala"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("a!"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(" "));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("a "));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(emptyData.string));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(extendedVoteBitsData.invalidLargeString));

    // vector validation

    std::vector<unsigned char> vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(vect));
    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(vect));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(extendedVoteBitsData.validLargeVector));

    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(emptyData.vector));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(extendedVoteBitsData.invalidLargeVector));

    // vote data size

    VoteData voteData;
    BOOST_CHECK_EQUAL(ExtendedVoteBits::minSize(), 1U);
    BOOST_CHECK_EQUAL(ExtendedVoteBits::maxSize(), nMaxStructDatacarrierBytes - GetVoteDataSizeWithEmptyExtendedVoteBits() - 1);
    BOOST_CHECK_EQUAL(GetVoteDataSizeWithEmptyExtendedVoteBits(), GetSerializeSize(GetScriptForVoteDecl(voteData), SER_NETWORK, PROTOCOL_VERSION));

    // default construction

    BOOST_CHECK(ExtendedVoteBits().getVector() == extendedVoteBitsData.empty.getVector());

    // construction from string

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits("00").getVector() == vect);

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits("01").getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits("0123").getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits("0123456789abcdef").getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString).getVector() == extendedVoteBitsData.validLargeVector);

    BOOST_CHECK(ExtendedVoteBits(emptyData.string).getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("0").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("1").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("123").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("12345").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("abc").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("lala").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("a!").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits(" ").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits("a ").getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.invalidLargeString).getVector() == extendedVoteBitsData.empty.getVector());

    // construction from vector (also testing vector access)

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeVector).getVector() == extendedVoteBitsData.validLargeVector);

    vect.clear();
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == extendedVoteBitsData.empty.getVector());

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector).getVector() == extendedVoteBitsData.empty.getVector());

    // copy constructor

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(extendedVoteBitsData.validLargeVector)).getVector() == extendedVoteBitsData.validLargeVector);

    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits()).getVector() == extendedVoteBitsData.empty.getVector());
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector)).getVector() == extendedVoteBitsData.empty.getVector());

    // move constructor

    vect = {0x00};
    ExtendedVoteBits evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    evb1 = ExtendedVoteBits(extendedVoteBitsData.validLargeVector);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == extendedVoteBitsData.validLargeVector);

    evb1 = ExtendedVoteBits();
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == extendedVoteBitsData.empty.getVector());

    evb1 = ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == extendedVoteBitsData.empty.getVector());

    // copy assignment

    vect = {0x00};
    evb1 = ExtendedVoteBits(vect);
    ExtendedVoteBits evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    evb1 = ExtendedVoteBits(extendedVoteBitsData.validLargeVector);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.validLargeVector);

    evb1 = ExtendedVoteBits();
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.empty.getVector());

    evb1 = ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.empty.getVector());

    // move assignment

    vect = {0x00};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    evb1 = ExtendedVoteBits(extendedVoteBitsData.validLargeVector);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.validLargeVector);

    evb1 = ExtendedVoteBits();
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.empty.getVector());

    evb1 = ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == extendedVoteBitsData.empty.getVector());

    // equality

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("00"));

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("0123"));

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("0123456789abcdef"));

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeVector) == ExtendedVoteBits(extendedVoteBitsData.validLargeString));
    BOOST_CHECK(ExtendedVoteBits() == extendedVoteBitsData.empty);
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector) == ExtendedVoteBits());

    // inequality

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(extendedVoteBitsData.validLargeString));

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(extendedVoteBitsData.validLargeString));

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(extendedVoteBitsData.validLargeString));

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(extendedVoteBitsData.validLargeString));

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString) != ExtendedVoteBits("0123456789abcdef"));

    // validity (also see vector access below)

    BOOST_CHECK(ExtendedVoteBits().isValid());
    BOOST_CHECK(ExtendedVoteBits("00").isValid());
    BOOST_CHECK(ExtendedVoteBits("01").isValid());
    BOOST_CHECK(ExtendedVoteBits("0123").isValid());
    BOOST_CHECK(ExtendedVoteBits("0123456789abcdef").isValid());
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeString).isValid());
    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.invalidLargeString).isValid());

    // vector access (mostly already tested in the constructor area above)
    // const_cast like this is highly discouraged

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()).clear();
    BOOST_CHECK(!evb1.isValid());

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()) = extendedVoteBitsData.validLargeVector;
    BOOST_CHECK(evb1.getVector() == extendedVoteBitsData.validLargeVector);

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()) = extendedVoteBitsData.invalidLargeVector;
    BOOST_CHECK(!evb1.isValid());

    // hexadecimal string

    BOOST_CHECK(ExtendedVoteBits().getHex() == "00");

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "00");

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "0123");

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "0123456789abcdef");

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.validLargeVector).getHex() == extendedVoteBitsData.validLargeString);

    BOOST_CHECK(ExtendedVoteBits(extendedVoteBitsData.invalidLargeVector).getHex() == "00");
}

// test the transaction for votes
BOOST_FIXTURE_TEST_CASE(vote_transaction, WalletStakeTestingSetup)
{
    std::string voteHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    // Premature votes and preconditions

    {
        // ticket hash
        std::tie(voteHashStr, we) = wallet->Vote(emptyData.hash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // stake validation height not reached yet
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());

        // block hash
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), emptyData.hash, chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->pprev->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // block height
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight - 1, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight + 1, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // ticket
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }

    // Single vote

    {
        BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::rttAccepted, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(voteHashStr.length() > 0);

        const uint256& voteHash = uint256S(voteHashStr);

        BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* vote = wallet->GetWalletTx(voteHash);
        BOOST_CHECK(vote != nullptr);
        BOOST_CHECK(vote->tx != nullptr);

        CheckVote(*vote->tx, *ticket->tx);

        ExtendChain(1);

        BOOST_CHECK(std::find_if(votesInLastBlock.begin(), votesInLastBlock.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != votesInLastBlock.end());
    }

    // Multiple votes

    {
        BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

        std::vector<uint256> voteHashes;

        for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners()) {
            BOOST_CHECK(ticketHash != emptyData.hash);

            std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::rttAccepted, extendedVoteBitsData.empty);
            BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
            BOOST_CHECK(voteHashStr.length() > 0);

            const uint256 voteHash = uint256S(voteHashStr);

            BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

            const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
            BOOST_CHECK(ticket != nullptr);
            BOOST_CHECK(ticket->tx != nullptr);

            const CWalletTx* vote = wallet->GetWalletTx(voteHash);
            BOOST_CHECK(vote != nullptr);
            BOOST_CHECK(vote->tx != nullptr);

            CheckVote(*vote->tx, *ticket->tx);

            voteHashes.push_back(voteHash);
        }

        ExtendChain(1);

        for (const uint256& voteHash : voteHashes)
            BOOST_CHECK(std::find_if(votesInLastBlock.begin(), votesInLastBlock.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != votesInLastBlock.end());
    }

    // Encrypted wallet

    {
        wallet->EncryptWallet(passphrase);
        BOOST_CHECK(wallet->IsLocked());

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::rttAccepted, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_UNLOCK_NEEDED);
    }
}

// test the transaction for votes with VSP support
BOOST_FIXTURE_TEST_CASE(vote_transaction_vsp, WalletStakeTestingSetup)
{
    std::string voteHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height(), true, true);

    BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

    std::vector<uint256> voteHashes;

    for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners()) {
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::rttAccepted, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(voteHashStr.length() > 0);

        const uint256 voteHash = uint256S(voteHashStr);

        BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* vote = wallet->GetWalletTx(voteHash);
        BOOST_CHECK(vote != nullptr);
        BOOST_CHECK(vote->tx != nullptr);

        CheckVote(*vote->tx, *ticket->tx);

        voteHashes.push_back(voteHash);
    }

    ExtendChain(1);

    for (const uint256& voteHash : voteHashes)
        BOOST_CHECK(std::find_if(votesInLastBlock.begin(), votesInLastBlock.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != votesInLastBlock.end());
}

// test the failing transaction for votes that spend ticket not belonging to the wallet
BOOST_FIXTURE_TEST_CASE(foreign_ticket_vote_transaction, WalletStakeTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight, true, false, true);

    BOOST_CHECK_GT(chainActive.Tip()->pstakeNode->Winners().size(), 0U);

    for (auto& hash : chainActive.Tip()->pstakeNode->Winners()) {
        std::tie(txHash, we) = wallet->Vote(hash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the failing transaction for votes that spend ticket not belonging to the wallet and using VSP
BOOST_FIXTURE_TEST_CASE(foreign_vsp_ticket_vote_transaction, WalletStakeTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight, true, true, true);

    BOOST_CHECK_GT(chainActive.Tip()->pstakeNode->Winners().size(), 0U);

    for (auto& hash : chainActive.Tip()->pstakeNode->Winners()) {
        std::tie(txHash, we) = wallet->Vote(hash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, VoteBits::allRejected, extendedVoteBitsData.empty);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the automatic voter
BOOST_FIXTURE_TEST_CASE(auto_voter, WalletStakeTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Premature votes and preconditions

    av->start();
    ExtendChain(1);
    BOOST_CHECK(votesInLastBlock.size() == 0U);
    ExtendChain(1);
    BOOST_CHECK(votesInLastBlock.size() == 0U);
    av->stop();

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());

    av->start();

    // All tickets (in mempool)

    ExtendChain(1);
    CheckLatestVotes(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1, false);
    CheckLatestVotes();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, false); // (from previous extension)
    BOOST_CHECK(ExtendChain(1, false) == false);
    BOOST_CHECK(votesInLastBlock.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    BOOST_CHECK(ExtendChain(1, false) == false);
    BOOST_CHECK(votesInLastBlock.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1);
    BOOST_CHECK(votesInLastBlock.size() > 0U);
    CheckLatestVotes(true);

    BOOST_CHECK(ExtendChain(1, false));
    BOOST_CHECK(votesInLastBlock.size() > 0U);
    CheckLatestVotes();

    av->stop();
}

// test the automatic voter with VSP support
BOOST_FIXTURE_TEST_CASE(auto_voter_vsp, WalletStakeTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Premature votes and preconditions

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height(), true, true);

    av->start();

    // All tickets (in mempool)

    ExtendChain(1, true, true);
    CheckLatestVotes(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1, false, true);
    CheckLatestVotes();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, false); // (from previous extension)
    BOOST_CHECK(ExtendChain(1, false, true) == false);
    BOOST_CHECK(votesInLastBlock.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    BOOST_CHECK(ExtendChain(1, false, true) == false);
    BOOST_CHECK(votesInLastBlock.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1, true, true);
    BOOST_CHECK(votesInLastBlock.size() > 0U);
    CheckLatestVotes(true);

    BOOST_CHECK(ExtendChain(1, false, true));
    BOOST_CHECK(votesInLastBlock.size() > 0U);
    CheckLatestVotes();

    av->stop();
}

// test the vote generating RPC
BOOST_FIXTURE_TEST_CASE(generate_vote_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();
    std::string blockHashStr = blockHash.GetHex();
    int blockHeight = chainActive.Tip()->nHeight;
    std::string blockHeightStr = std::to_string(blockHeight);
    BOOST_CHECK_EQUAL(chainActive.Tip()->pstakeNode->Winners().size(), consensus.nTicketsPerBlock);
    uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
    std::string ticketHashStr = ticketHash.GetHex();

    // missing required parameters

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr), std::runtime_error);

    // block hash

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + "0" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + "test" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + "abc!" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + "123" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);

    // block height

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + "0" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + "test" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + "abc!" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + "123" + " " + ticketHashStr + " " + "1"), std::runtime_error);

    // ticket hash

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "0" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "test" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "abc!" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "123" + " " + "1"), std::runtime_error);

    // vote bits

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "-1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "65536"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "test"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "abc!"), std::runtime_error);

    // vote yes

    UniValue r;

    BOOST_CHECK_NO_THROW(r = CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1"));
    BOOST_CHECK(find_value(r.get_obj(), "hex").isStr());

    uint256 voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits::rttAccepted, 0, extendedVoteBitsData.empty});

    // vote no

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[1];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "0"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits::allRejected, 0, extendedVoteBitsData.empty});

    // vote exhaustive

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[2];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "65535"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits(65535U), 0, extendedVoteBitsData.empty});

    // extended vote bits

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[3];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "123"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "abcde"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "test"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "abc!"), std::runtime_error);

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + extendedVoteBitsData.invalidLargeString), std::runtime_error);

    BOOST_CHECK_NO_THROW(r = CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "1234567890abcdef"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits::rttAccepted, 0, ExtendedVoteBits("1234567890abcdef")});

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[4];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + extendedVoteBitsData.validLargeString));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits::rttAccepted, 0, ExtendedVoteBits(extendedVoteBitsData.validLargeString)});

    // encrypted wallet

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1);

    blockHash = chainActive.Tip()->GetBlockHash();
    blockHashStr = blockHash.GetHex();
    blockHeight = chainActive.Tip()->nHeight;
    blockHeightStr = std::to_string(blockHeight);
    BOOST_CHECK_EQUAL(chainActive.Tip()->pstakeNode->Winners().size(), consensus.nTicketsPerBlock);
    ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_THROW(CallRpc(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + extendedVoteBitsData.empty.getHex()), std::runtime_error);

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

// test the automatic voter RPCs
BOOST_FIXTURE_TEST_CASE(auto_voter_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    CPubKey ticketPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
    CTxDestination ticketKeyId = ticketPubKey.GetID();
    std::string ticketAddress{EncodeDestination(ticketKeyId)};

    CPubKey vspPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    CTxDestination vspKeyId = vspPubKey.GetID();
    std::string vspAddress{EncodeDestination(vspKeyId)};

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Settings (write)

    BOOST_CHECK_THROW(CallRpc("setautovotervotebits"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("setautovotervotebits abc"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("setautovotervotebits -1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("setautovotervotebits 65536"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebits 0"));
    BOOST_CHECK(cfg.voteBits == VoteBits(0U));
    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebits 1234"));
    BOOST_CHECK(cfg.voteBits == VoteBits(1234U));
    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebits 65535"));
    BOOST_CHECK(cfg.voteBits == VoteBits(65535U));
    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebits 1"));
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));

    BOOST_CHECK_THROW(CallRpc("setautovotervotebitsext"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("setautovotervotebitsext "), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("setautovotervotebitsext  "), std::runtime_error);

    std::string largeExtendedVoteBits;
    for (unsigned i = 0; i <= ExtendedVoteBits::maxSize(); ++i)
        largeExtendedVoteBits += "00";
    BOOST_CHECK_THROW(CallRpc("setautovotervotebitsext " + largeExtendedVoteBits), std::runtime_error);

    BOOST_CHECK(cfg.extendedVoteBits == extendedVoteBitsData.empty);

    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebitsext 00"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("00"));
    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebitsext 1234"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("1234"));
    BOOST_CHECK_NO_THROW(CallRpc("setautovotervotebitsext 12345678900abcdef0"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("12345678900abcdef0"));

    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("12345678900abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Settings (read)

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRpc("autovoterconfig"));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "autovote").get_bool(), false);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votebits").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votebitsext").get_str(), "12345678900abcdef0");

    // Start (with minimal settings)

    BOOST_CHECK_THROW(CallRpc("startautovoter"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautovoter \"\""), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("startautovoter 65535"));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(65535U));
    BOOST_CHECK(cfg.extendedVoteBits == extendedVoteBitsData.empty);
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Start (with full settings)

    av->stop();

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    BOOST_CHECK_NO_THROW(CallRpc("startautovoter 1234 123456789abcdef0"));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1234U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("123456789abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Stop

    BOOST_CHECK_NO_THROW(CallRpc("stopautovoter"));

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    // Start with passphrase

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(CallRpc("startautovoter 1234 123456789abcdef0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautovoter 1234 123456789abcdef0 wrongP4ssword!"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRpc(std::string("startautovoter 1 123456789abcdef0 ") + std::string(passphrase.c_str())));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("123456789abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    av->stop();

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    BOOST_CHECK_NO_THROW(CallRpc(std::string("startautovoter 1  ") + std::string(passphrase.c_str())));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == extendedVoteBitsData.empty);
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    av->stop();

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

BOOST_AUTO_TEST_SUITE_END()
