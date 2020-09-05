// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_stake_test_fixture.h"

#include <boost/test/unit_test.hpp>

#include "validation.h"
#include "rpc/server.h"

BOOST_FIXTURE_TEST_SUITE(revoke_tests, WalletStakeTestingSetup)

// test the revocation estimated sizes of declaration output and whole transaction
BOOST_AUTO_TEST_CASE(revoke_estimated_sizes)
{
    BOOST_CHECK_EQUAL(GetEstimatedRevokeTicketDeclTxOutSize(), 16U);

    BOOST_CHECK_EQUAL(GetEstimatedSizeOfRevokeTicketTx(0), 175U);
    BOOST_CHECK_EQUAL(GetEstimatedSizeOfRevokeTicketTx(0, false), 207U);

    BOOST_CHECK_EQUAL(GetEstimatedSizeOfRevokeTicketTx(1), 209U);
    BOOST_CHECK_EQUAL(GetEstimatedSizeOfRevokeTicketTx(2), 243U);
    BOOST_CHECK_EQUAL(GetEstimatedSizeOfRevokeTicketTx(10), 515U);
}

// test the calculation of gross and net remunerations
BOOST_AUTO_TEST_CASE(remuneration_calculations)
{
    struct TestData {
        std::vector<TicketContribData> contributions;
        CAmount stake;
        CAmount subsidy;
        CAmount fee;
        FeeDistributionPolicy feePolicy;
        std::vector<CAmount> expectedRemunerations;
    };

    CPubKey pubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(pubKey));
    const CTxDestination addr = pubKey.GetID();

    std::vector<TestData> tests {

        // stake

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)},
            100 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {100 * COIN}
        },
        {
            {TicketContribData(1, addr, 123 * COIN, MAX_MONEY, MAX_MONEY)},
            123 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {123 * COIN}
        },
        {
            {TicketContribData(1, addr, 2000 * COIN, MAX_MONEY, MAX_MONEY)},
            2000 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {2000 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {100 * COIN, 100 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {50 * COIN, 150 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            300 * COIN, 0 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {50 * COIN, 150 * COIN, 100 * COIN}
        },

        // subsidy

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)},
            0 * COIN, 1500 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {1500 * COIN}
        },
        {
            {TicketContribData(1, addr, 500 * COIN, MAX_MONEY, MAX_MONEY)},
            0 * COIN, 100 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {100 * COIN}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)},
            0 * COIN, 100 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {100 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            0 * COIN, 200 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {100 * COIN, 100 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY)
            },
            0 * COIN, 200 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {13333333333, 6666666667}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            0 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {16666666666, 50000000000, 33333333334}
        },

        // stake + subsidy

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {1100 * COIN}
        },
        {
            {TicketContribData(1, addr, 123 * COIN, MAX_MONEY, MAX_MONEY)},
            123 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {1123 * COIN}
        },
        {
            {TicketContribData(1, addr, 2000 * COIN, MAX_MONEY, MAX_MONEY)},
            2000 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {3000 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {600 * COIN, 600 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {300 * COIN, 900 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            300 * COIN, 1000 * COIN, 0 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {21666666666, 65000000000, 43333333334}
        },

        // fee

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {1050 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {575 * COIN, 575 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            300 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {20833333333, 62500000000, 41666666667}
        },

        // fee limits (votes)

        {
            {TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 4096, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {105000000000}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {551 * COIN, 599 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 2 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)
            },
            300 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {21466666666, 60300000001, 43233333333}
        },

        // fee limits (revocations)

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)},
            100 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 4096)},
            100 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, TicketContribData::DefaultFeeLimit)},
            100 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)},
            100 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)},
            100 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {50 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, TicketContribData::DefaultFeeLimit),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, TicketContribData::DefaultFeeLimit)
            },
            200 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {5001048576, 9998951424}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)
            },
            200 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {51 * COIN, 99 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 2 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, TicketContribData::DefaultFeeLimit)
            },
            300 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {48 * COIN, 10201048576, 9998951424}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 2 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)
            },
            300 * COIN, 0 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {48 * COIN, 103 * COIN, 99 * COIN}
        },

        // fee distribution policy (proportional mostly tested above)

        {
            {TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 4096, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {1050 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {599 * COIN, 551 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {550 * COIN, 600 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {550 * COIN, 600 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 2 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {198 * COIN, 553 * COIN, 399*COIN}
        },

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY ,4096)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {50 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {99 * COIN, 51 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {50 * COIN, 100 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {50 * COIN, 100 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 2 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {3133333333, 5300000001, 6566666666}
        },

        {
            {
                TicketContribData(1, addr, 50 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {150 * COIN, 600 * COIN, 400 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::FirstContributor,
            {150 * COIN, 600 * COIN, 400 * COIN}
        },

        {
            {TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 4096, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)},
            100 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {105000000000}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {599 * COIN, 551 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 50 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {575 * COIN, 575 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {575 * COIN, 575 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 1 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 0, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 0, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 2 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 50 * COIN, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 1 * COIN, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {198 * COIN, 553 * COIN, 399 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, 1666666666, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, 1666666668, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, 1666666666, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {18333333334, 58333333332, 38333333334}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {18333333334, 58333333334, 38333333332}
        },

        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 4096)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)},
            100 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {50 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {99 * COIN, 51 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 50 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {75 * COIN, 75 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {75 * COIN, 75 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 1 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 0),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 0)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 2 * COIN),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 50 * COIN),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1 * COIN)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {3133333333, 5300000001, 6566666666}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, 1666666666),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, 1666666668),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, 1666666666)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {1666666667, 8333333333, 50 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::EqualFee,
            {1666666667, 8333333334, 4999999999}
        },

        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 60 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {190 * COIN, 570 * COIN, 380 * COIN}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 1000 * COIN, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {19166666667, 57500000000, 38333333333}
        },

        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0, 60 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {2333333333, 70 * COIN, 4666666667}
        },
        {
            {
                TicketContribData(1, addr, 50 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 150 * COIN, MAX_MONEY, MAX_MONEY),
                TicketContribData(1, addr, 100 * COIN, MAX_MONEY, MAX_MONEY)
            },
            200 * COIN, 0, 50 * COIN,
            FeeDistributionPolicy::ProportionalFee,
            {25 * COIN, 75 * COIN, 50 * COIN}
        }
    };

    for (auto& test: tests)
        BOOST_CHECK(CalculateNetRemunerations(test.contributions, test.stake, test.subsidy, test.fee, test.feePolicy) == test.expectedRemunerations);
}

// test the verification of the ticket revocation being in mempool
BOOST_AUTO_TEST_CASE(is_ticket_revoked_in_mempool)
{
    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, false, false, true);

    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    for (auto& ticketHash : chainActive.Tip()->pstakeNode->MissedTickets()) {
        AddRevokeTx(ticketHash);
        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));
    }
}

// test the transaction for revocation
BOOST_FIXTURE_TEST_CASE(revocation_transaction, WalletStakeTestingSetup)
{
    std::string revocationHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    // Premature revocations and preconditions

    {
        // ticket hash
        std::tie(revocationHashStr, we) = wallet->Revoke(emptyData.hash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // stake validation height not reached yet
        std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->GetBlockHash());
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, false, false, true);
        while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
            ExtendChain(1, true, false, false, true);

        // invalid ticket
        std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->GetBlockHash());
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // already voted ticket
        std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->pprev->pstakeNode->Winners()[0]);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // winner
        std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->pstakeNode->Winners()[0]);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // correct revocation
        std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->pstakeNode->MissedTickets()[0]);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        CheckRevocation(uint256S(revocationHashStr), chainActive.Tip()->pstakeNode->MissedTickets()[0]);

        // revoked in mempool
        //std::tie(revocationHashStr, we) = wallet->Revoke(chainActive.Tip()->pstakeNode->MissedTickets()[0]);
        //BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }

    // Single revocation

    {
        do {
            ExtendChain(1, true, false, false, true);
        } while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0);

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->MissedTickets()[0];
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(revocationHashStr, we) = wallet->Revoke(ticketHash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(revocationHashStr.length() > 0);

        const uint256& revocationHash = uint256S(revocationHashStr);

        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* revocation = wallet->GetWalletTx(revocationHash);
        BOOST_CHECK(revocation != nullptr);
        BOOST_CHECK(revocation->tx != nullptr);

        CheckRevocation(*revocation->tx, *ticket->tx);

        ExtendChain(1, true, false, false, true);

        BOOST_CHECK(std::find_if(revocationsInLastBlock.begin(), revocationsInLastBlock.end(), [&revocationHash](const CTransactionRef tx) { return tx->GetHash() == revocationHash; }) != revocationsInLastBlock.end());
    }

    // Multiple revocations

    {
        do {
            ExtendChain(1, true, false, false, true);
        } while (chainActive.Tip()->pstakeNode->MissedTickets().size() < 5);

        std::vector<uint256> revocationHashes;

        for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->MissedTickets()) {
            BOOST_CHECK(ticketHash != emptyData.hash);

            std::tie(revocationHashStr, we) = wallet->Revoke(ticketHash);
            BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
            BOOST_CHECK(revocationHashStr.length() > 0);

            const uint256 revocationHash = uint256S(revocationHashStr);

            BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));

            const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
            BOOST_CHECK(ticket != nullptr);
            BOOST_CHECK(ticket->tx != nullptr);

            const CWalletTx* revocation = wallet->GetWalletTx(revocationHash);
            BOOST_CHECK(revocation != nullptr);
            BOOST_CHECK(revocation->tx != nullptr);

            CheckRevocation(*revocation->tx, *ticket->tx);

            revocationHashes.push_back(revocationHash);
        }

        ExtendChain(1, true, false, false, true);

        for (const uint256& revocationHash : revocationHashes)
            BOOST_CHECK(std::find_if(revocationsInLastBlock.begin(), revocationsInLastBlock.end(), [&revocationHash](const CTransactionRef tx) { return tx->GetHash() == revocationHash; }) != revocationsInLastBlock.end());
    }

    // Encrypted wallet

    {
        wallet->EncryptWallet(passphrase);
        BOOST_CHECK(wallet->IsLocked());

        while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
            ExtendChain(1, true, false, false, true);

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->MissedTickets()[0];
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(revocationHashStr, we) = wallet->Revoke(ticketHash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_UNLOCK_NEEDED);
    }
}

// test the transaction for revocations with VSP support
BOOST_FIXTURE_TEST_CASE(revocation_transaction_vsp, WalletStakeTestingSetup)
{
    std::string revocationHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, true, false, true);

    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);

    std::vector<uint256> revocationHashes;

    for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->MissedTickets()) {
        BOOST_CHECK(ticketHash != emptyData.hash);

        std::tie(revocationHashStr, we) = wallet->Revoke(ticketHash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(revocationHashStr.length() > 0);

        const uint256 revocationHash = uint256S(revocationHashStr);

        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* revocation = wallet->GetWalletTx(revocationHash);
        BOOST_CHECK(revocation != nullptr);
        BOOST_CHECK(revocation->tx != nullptr);

        CheckRevocation(*revocation->tx, *ticket->tx);

        revocationHashes.push_back(revocationHash);
    }

    ExtendChain(1, true, true, false, true);

    for (const uint256& revocationHash : revocationHashes)
        BOOST_CHECK(std::find_if(revocationsInLastBlock.begin(), revocationsInLastBlock.end(), [&revocationHash](const CTransactionRef tx) { return tx->GetHash() == revocationHash; }) != revocationsInLastBlock.end());
}

// test the failure to create revocations for tickets not belonging to the wallet
BOOST_FIXTURE_TEST_CASE(foreign_ticket_revoke_transaction, WalletStakeTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, false, true, true, true);

    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);

    for (auto& hash : chainActive.Tip()->pstakeNode->MissedTickets()) {
        std::tie(txHash, we) = wallet->Revoke(hash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the failure to create revocations for tickets not belonging to the wallet and using VSP
BOOST_FIXTURE_TEST_CASE(foreign_vsp_ticket_revoke_transaction, WalletStakeTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, true, true, true, true);

    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);

    for (auto& hash : chainActive.Tip()->pstakeNode->MissedTickets()) {
        std::tie(txHash, we) = wallet->Revoke(hash);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the revocation of all tickets
BOOST_FIXTURE_TEST_CASE(revoke_all, WalletStakeTestingSetup)
{
    std::vector<std::string> revocationHashes;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Tip()->nHeight, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() < 5)
        ExtendChain(1, true, false, false, true);

    std::tie(revocationHashes, we) = wallet->RevokeAll();
    BOOST_CHECK(we.code == CWalletError::SUCCESSFUL);
    BOOST_CHECK(revocationHashes.size() != 0);
    BOOST_CHECK(revocationHashes.size() == chainActive.Tip()->pstakeNode->MissedTickets().size());

    ExtendChain(1, true, false, false, true);

    BOOST_CHECK(revocationHashes.size() == revocationsInLastBlock.size());

    BOOST_CHECK(std::find_if(revocationsInLastBlock.begin(), revocationsInLastBlock.end(), [&revocationHashes](const CTransactionRef tx) {
        return std::find(revocationHashes.begin(), revocationHashes.end(), tx->GetHash().GetHex()) != revocationHashes.end();
    }) != revocationsInLastBlock.end());
}

// test the automatic revoker
BOOST_FIXTURE_TEST_CASE(auto_revoker, WalletStakeTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoRevoker* ar = wallet->GetAutoRevoker();
    BOOST_CHECK(ar != nullptr);

    BOOST_CHECK(!ar->isStarted());

    CAutoRevokerConfig& cfg = ar->GetConfig();

    // Premature revocations and preconditions

    ar->start();
    ExtendChain(1);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);
    ExtendChain(1);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);
    ar->stop();

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    ar->start();

    // All tickets (in mempool)

    ExtendChain(1);
    CheckLatestRevocations(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1);
    CheckLatestRevocations();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, true, false, false, true);
    ExtendChain(1, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    ExtendChain(1, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);
    CheckLatestRevocations(true);

    ExtendChain(1);
    BOOST_CHECK(revocationsInLastBlock.size() > 0U);
    CheckLatestRevocations();

    ar->stop();
}

// test the automatic revoker with VSP support
BOOST_FIXTURE_TEST_CASE(auto_revoker_vsp, WalletStakeTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoRevoker* ar = wallet->GetAutoRevoker();
    BOOST_CHECK(ar != nullptr);

    BOOST_CHECK(!ar->isStarted());

    CAutoRevokerConfig& cfg = ar->GetConfig();

    // Premature revocations and preconditions

    ar->start();
    ExtendChain(1, true, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);
    ExtendChain(1, true, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);
    ar->stop();

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height(), true, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);

    ar->start();

    // All tickets (in mempool)

    ExtendChain(1, true, true);
    CheckLatestRevocations(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1, true, true);
    CheckLatestRevocations();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, true, true, false, true);
    ExtendChain(1, true, true, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    ExtendChain(1, true, true, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);
    BOOST_CHECK(revocationsInLastBlock.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1, true, true, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, true, false, true);
    CheckLatestRevocations(true);

    ExtendChain(1, true, true);
    BOOST_CHECK(revocationsInLastBlock.size() > 0U);
    CheckLatestRevocations();

    ar->stop();
}

// test the single ticket revocation RPC
BOOST_FIXTURE_TEST_CASE(revoke_ticket_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    // parameters

    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket true")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket abc")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket abc def")), std::runtime_error);

    // ticket hash

    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket 0")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket 1")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket ") + emptyData.hash.GetHex()), std::runtime_error);

    std::vector<unsigned char> rnd(32);
    GetStrongRandBytes(rnd.data(), 32);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketicket ") + uint256(rnd).GetHex()), std::runtime_error);

    // full revocation

    for (const uint256& ticketHash: chainActive.Tip()->pstakeNode->MissedTickets()) {
        BOOST_CHECK_NO_THROW(CallRpc(std::string("revoketicket ") + ticketHash.GetHex()));
        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));
    }

    // Encrypted wallet

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    for (const uint256& ticketHash: chainActive.Tip()->pstakeNode->MissedTickets())
        BOOST_CHECK_THROW(CallRpc(std::string("revoketicket ") + ticketHash.GetHex()), std::runtime_error);

    wallet->Unlock(passphrase);
    BOOST_CHECK(!wallet->IsLocked());

    for (const uint256& ticketHash: chainActive.Tip()->pstakeNode->MissedTickets()) {
        BOOST_CHECK_NO_THROW(CallRpc(std::string("revoketicket ") + ticketHash.GetHex()));
        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));
    }

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

// test the RPC for the revocation of all tickets
BOOST_FIXTURE_TEST_CASE(revoke_tickets_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    // parameters

    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets 0")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets true")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets abc")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets abc def")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets 0")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets 1")), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets ") + emptyData.hash.GetHex()), std::runtime_error);

    std::vector<unsigned char> rnd(32);
    GetStrongRandBytes(rnd.data(), 32);
    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets ") + uint256(rnd).GetHex()), std::runtime_error);

    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets ") + chainActive.Tip()->pstakeNode->MissedTickets()[0].GetHex()), std::runtime_error);

    // full revocation

    BOOST_CHECK_NO_THROW(CallRpc(std::string("revoketickets")));

    for (const uint256& ticketHash: chainActive.Tip()->pstakeNode->MissedTickets())
        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));

    // Encrypted wallet

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, true, false, false, true);
    while (chainActive.Tip()->pstakeNode->MissedTickets().size() == 0)
        ExtendChain(1, true, false, false, true);

    BOOST_CHECK_THROW(CallRpc(std::string("revoketickets")), std::runtime_error);

    wallet->Unlock(passphrase);
    BOOST_CHECK(!wallet->IsLocked());

    BOOST_CHECK_NO_THROW(CallRpc(std::string("revoketickets")));

    for (const uint256& ticketHash: chainActive.Tip()->pstakeNode->MissedTickets())
        BOOST_CHECK(wallet->IsTicketRevokedInMempool(ticketHash));

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

// test the automatic revoker RPCs
BOOST_FIXTURE_TEST_CASE(auto_revoker_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    CAutoRevoker* ar = wallet->GetAutoRevoker();
    BOOST_CHECK(ar != nullptr);

    BOOST_CHECK(!ar->isStarted());

    CAutoRevokerConfig& cfg = ar->GetConfig();

    // Settings

    UniValue r;
    BOOST_CHECK_THROW(r = CallRpc("autorevokerconfig 0"), std::runtime_error);
    BOOST_CHECK_THROW(r = CallRpc("autorevokerconfig true"), std::runtime_error);
    BOOST_CHECK_THROW(r = CallRpc("autorevokerconfig abc"), std::runtime_error);

    BOOST_CHECK_NO_THROW(r = CallRpc("autorevokerconfig"));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "autorevoke").get_bool(), false);
    BOOST_CHECK(!cfg.autoRevoke);

    // Start

    BOOST_CHECK_THROW(CallRpc("startautorevoker 0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautorevoker true"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautorevoker abc"), std::runtime_error);

    BOOST_CHECK(!ar->isStarted());

    BOOST_CHECK_NO_THROW(CallRpc("startautorevoker"));
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    BOOST_CHECK(ar->isStarted());

    // Stop

    BOOST_CHECK_NO_THROW(CallRpc("stopautorevoker"));

    BOOST_CHECK(!ar->isStarted());

    // Start with passphrase

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(CallRpc("startautorevoker 0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautorevoker true"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautorevoker abc"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startautorevoker aWr0ngP4$$word"), std::runtime_error);

    BOOST_CHECK(!ar->isStarted());

    BOOST_CHECK_NO_THROW(CallRpc(std::string("startautorevoker ") + std::string(passphrase.c_str())));

    BOOST_CHECK(ar->isStarted());

    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    ar->stop();

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

BOOST_AUTO_TEST_SUITE_END()
