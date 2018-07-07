// Copyright (c) 2018 by ObEN, Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "investor.h"
#include <map>
#include <assert.h>
#include <time.h>
#include <algorithm>
#include "chainparams.h"
#include "base58.h"
#include "wallet.h"
#include "script/standard.h"
#include "fees.h"
#include <core_io.h>
#include "policy/policy.h"

Investor& Investor::GetInstance()
{
    static Investor instance;
    return instance;
}

Investor::Investor()
{
    std::vector<unsigned char> privKeyVector;
    CKey privKey;

    // mainnet
    if (Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS)[0] == 56) {
        privKeyVector = ParseHex("998aaf457ccc93c1a53d441b543873a7b06302da5f8be312545e18d0695fc085");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521244800,  // March 17, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("02329cc927431609500d37dab984d9623a90e0a5e7051fd55216e7e2cd00fc305d")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("f528921e82974e6558661f38ca3d9115500cf8d5bd1864b5a3e12b97ec549204");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521331200,  // March 18, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("020347e6a5cf42c5d452af47d04784c352ab2961befc2c917c9099b5e0c1780359")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("7c35252be5a8f611df846e2c4845027c3dc9899ba6486660d16f5c3cc2900ef1");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521417600,  // March 19, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("03fe4a6841ee7f6e78c12a03ad2cfb7a290748151a70ff61d70a98ce4ed7ffdecb")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("6a4fdc702ecfa9faec92d51d959d881c1780d08262e142a72f90409075b25a58");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521504000,  // March 20, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("035312bbe2c2937eb059871ce57b147625e7c388f319855e5c952d17aa29ad2b63")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("e24f7ac3dbcce1f8e5e70f4bbf61a7fdd2a26f499d8bdcd85d1a56a3e281ae66");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521590400,  // March 21, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("03c2307de5b5e89bbb940c8773940c046aafe11b0874f5b82c2a2c4ae9bfde2b92")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("e79dc8a7ff42bc98b43ef7ce9258379eea60c57860713a73da7396aa650cd43e");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521676800,  // March 22, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("039fdd74bd21c32011bd79d2a1b6e7d30d9cdeeafb887f9dd58c5183fb39b9c69f")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("93a345c257bbf437b0d08cadf423483eefcb2c4bf0fdea1885d40765efb2de32");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521763200,  // March 23, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("04ed69725ef7d3dba78662fdf61e8529a7a6ad84d013890cdd0d05fbb03529cdd663a9abd6bbf448cc8f118c3020a667cb537b6bf66fed246b95085354f6fe52c0")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("025e0145a1d4881bf7123e5dd6ff9a4fac7a5497d00305de43b0b1c1452c3ecc");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521849600,  // March 24, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("04a0028aebf46285a4bad45093b1a4e0d913bbdca10874a868bbe517e919a9fd5bc3db0cfc0006b85b9a4dc8b2b2cd05a37debed06e697d969dd07d40e55add25c")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("a0cff2e13b74234ddec423dbf17664c12a0088fe8739d715218fc82e091c11bc");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521936000,  // March 25, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("04f1d51205c3f15baf57af38c4c95a1f5b1a5800689d08ddfdc45770fd63d7f8a62d6bcb10780412cf73ca29b1264869f808d59c4319c36332ac08bd7014589df7")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("373980ac51ba168b84654c38fc2f11140159a2e18c9dc48d0575049a7a6580ee");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1522022400,  // March 26, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("048a3c00af4705a284d033936dd702b4ce24a042fdd1a2097125aad2028d73da46f4927a479cdeff175d61af4b277dceb872715cd1cf8f5d3b7ea3427079fb6e86")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1524268800,  // April 21, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("047f588c84ca36d37a4144b766414d72b32439661b03d59b575476aec59696379eb48dfa0b787be359987503b74719a2eeb32a482d115bd4bd6c2b760a592656e0")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1529452800,  // June 20, 2018, 00:00:00 GMT
                                   .paiPublicKey = CPubKey(ParseHex("048a4c20d780afc2e482fff89932bb3a84843ff04fce0a537a98926f215f1138c60b446001eb532770deb9309fde543cb86dc7a98fb36eb767f5fae811a63981f4")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});
    }

    // testnet or regtest
    else {
        privKeyVector = ParseHex("2d23cd993314116f26ef2ca59bf328fcab7aff938cd124ccaafca53c8381f07b");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520553600,
                                   .paiPublicKey = CPubKey(ParseHex("02ab15a381a076b651c1cedb44f092b5b22dfeba4d992e9a486f8b1817efb884eb")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("1913898777938eb55365df14e507e737a691d3cc65fc55820a8c0671b65467f2");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520640000,
                                   .paiPublicKey = CPubKey(ParseHex("020ec61e077f113a8f09e08ef55933607ffc5ff3c03cb1bd7bf958ac8f7c46de5a")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("dd1d85cb8dc57f8f8580f184b97cd9041a820c984b1d592050a56212de4d6a8e");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520726400,
                                   .paiPublicKey = CPubKey(ParseHex("0234346d6aff8f0bb00b92c5481a2327b87a42042637b1cafbac7b380b1292d95d")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("c5843bae53e2ed85ba5279f7b8b01a2e1983fa588765a35f61ae8cd18b14bff7");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), true);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520812800,
                                   .paiPublicKey = CPubKey(ParseHex("02d0be112aaa07a505f9d5e1d187d529768e1920b0eb1f1db0985e032582f35bad")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("77b2c82926cd6a408fb771f7e1c266a4390169d67c043ee192d035b8b5bdae0c");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520899200,
                                   .paiPublicKey = CPubKey(ParseHex("041d256766c1bcc4c5f813659c9859431a7b6be547d4c1fd151180308409ab50d18d88af8927b0ea8569754deabd502faf858395cd1c157c698508c9fc27e422d4")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("f7fa92f75621f3996d4deb163fc961fe957aa163b748124c93b10964a6077bea");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1520985600,
                                   .paiPublicKey = CPubKey(ParseHex("042711c27869e3b22e6315d500675b5d5e7aa3977019aade9bc11ac39b2b2482e5cfb6ea22bd7ff4e47bf3499be04da1dd57f178c13b01e8dba901ec3dc2909516")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("ca1df1606be5eda42e1138ec94597a38da6bff1068ba6e5daf0492d99d20b651");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521072000,
                                   .paiPublicKey = CPubKey(ParseHex("04edb03063e7cb63ca50abda61878ea7e36771cf1cd02ae6e2331fdbae028c8c349bc2a598b03affcf770858bed23061a4bb8befaf2ef67ce6f65c5d3c3d0f442b")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("46d5e4afd1240bda94398eb05941b443b4796d7479709e9cd5592bc1e275325a");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521158400,
                                   .paiPublicKey = CPubKey(ParseHex("04854cf7d5afe83cef0ea2bad382c16704be47f3353895eb9096d6467d79290fb2b3e8a7661739993d80e79228f2f45881a226f6964896060e32286565d86e05b3")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("a813c5053f08791899faa0f90dffa18bdfe3f9f6d1bfeb1a56425d35e8be4df7");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521244800,
                                   .paiPublicKey = CPubKey(ParseHex("0473ef033236d4090d8d0ba5465d425573e301fdcc72bd8b1a3fb845df5bb3066cf1287e26c6cc71ab9e223127d1d084a72411ccba47ceac0a72496b5067425bd3")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});

        privKeyVector = ParseHex("277690f9977de9de9e1b4d61e417f2a4cc51711bdf771895c77dedd18966afb0");
        privKey.Set(privKeyVector.begin(), privKeyVector.end(), false);
        HoldingPeriods.push_back( {.expirationTimestamp = 1521331200,
                                   .paiPublicKey = CPubKey(ParseHex("041a4f22d052be27017e5d7e4868758647bb5b326c5d3b19644b43824874b2c4cb53eb58bf8478f36fde88b0b3b0eef6d5a54011ed35be8a21db65ebbeb0e20c57")),
                                   .paiPrivateKey = privKey,
                                   .investorPublicKey = CPubKey(),
                                   .multisigAddress = std::string(""),
                                   .redeemScript = CScript(),
                                   .balance = 0});
    }
}

Investor::~Investor()
{
}

// utilities

void Investor::AddTxHash(const CWalletTx *tx)
{
    if (IsTxHashAdded(tx))
        return;

    addedTxHashes.push_back(tx->GetHash());
}

void Investor::DeleteTxHash(const CWalletTx *tx)
{
    addedTxHashes.erase(std::remove_if(addedTxHashes.begin(), addedTxHashes.end(), [tx](uint256 x) { return (tx->GetHash() == x); }), addedTxHashes.end());
}

bool Investor::IsTxHashAdded(const CWalletTx *tx)
{
    return (std::find(addedTxHashes.begin(), addedTxHashes.end(), tx->GetHash()) != addedTxHashes.end());
}

void Investor::CleanTxHashList(void)
{
    addedTxHashes.clear();
}

bool Investor::DoesScriptPayToMultisigAddress(const CScript& script, const std::string& multisigAddress)
{
    if (multisigAddress.size() == 0) {
        return false;
    }

    if (!script.IsPayToScriptHash()) {
        return false;
    }

    CTxDestination txDestination;
    std::string scriptAddress;
    if (ExtractDestination(script, txDestination))
    {
        scriptAddress = EncodeDestination(txDestination);
    }

    if ((scriptAddress.size() == 0) || (multisigAddress != scriptAddress)) {
        return false;
    }

    return true;
}

bool Investor::FundingOutputForInput(const CWallet& wallet, const CTxIn& input, CTxOut& output)
{
    // since the actual amount and address are not present in the input, search for the funding transaction and get the appropriate output's amount and address
    const CWalletTx *fundingTx = wallet.GetWalletTx(input.prevout.hash);

    if (fundingTx && (input.prevout.n < fundingTx->tx->vout.size())) {
        output = fundingTx->tx->vout[input.prevout.n];
        return true;
    }

    return false;
}

bool Investor::MultisigFundingOutputForInput(const CWallet& wallet, const CTxIn& input, const std::string& multisigAddress, CTxOut& output)
{
    CTxOut txOut;

    if (!FundingOutputForInput(wallet, input, txOut)) {
        return false;
    }

    if (DoesScriptPayToMultisigAddress(txOut.scriptPubKey, multisigAddress)) {
        output = txOut;
        return true;
    }

    return false;
}

// multisig address

bool Investor::CreateMultisig(std::string& address, CScript& redeemScript, const int required, const std::vector<CPubKey>& pubKeys)
{
    // validation

    assert((required >= 0) && (required <= 16) && (pubKeys.size() <= 16));

    if ((required > 16) || (pubKeys.size() > 16)) {
        return false;
    }

    // script

    redeemScript.clear();

    redeemScript += GetScriptForMultisig(required, pubKeys);

    // address

    std::vector<unsigned char> vaddr;
    vaddr.resize(21 - 1);

    std::vector<unsigned char> prefix = Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    vaddr.insert(vaddr.begin(), prefix.begin(), prefix.begin() + 1);

    CHash160 hash160;
    hash160.Write(&redeemScript[0], redeemScript.size());
    hash160.Finalize(&vaddr[1]);

    address = EncodeBase58Check(vaddr);

    return true;
}

// keys

void Investor::SetPublicKey(CWallet& wallet, const CPubKey& pubKey)
{
    LOCK(csInvestor);

    bool updated = false;
    for (auto&& period : HoldingPeriods) {
        // check whether the public key is different and skip if so, to avoid a potential deadlock
        // note that if at least one public key needs update, then the deadlock might still occur
        if (period.investorPublicKey == pubKey) {
            continue;
        }

        period.investorPublicKey = pubKey;

        updated = true;

        std::vector<CPubKey> pubKeys;
        pubKeys.push_back(pubKey);
        pubKeys.push_back(period.paiPublicKey);

        CreateMultisig(period.multisigAddress, period.redeemScript, 2, pubKeys);

        period.balance = 0;
    }

    // refresh the investor balance only if there was an update in a public key
    if (updated) {
        UpdateGlobalBalance(wallet);
    }
}

// all addreses

std::vector<std::string> Investor::AllMultisigAddresses()
{
    std::vector<std::string> addresses;
    
    LOCK(csInvestor);
    
    for (auto&& period : HoldingPeriods) {
        addresses.push_back(period.multisigAddress);
    }

    return addresses;
}

// balance

uint64_t Investor::GlobalBalance(void)
{
   LOCK(csInvestor);
    
    uint64_t balance = 0;
    
    for (auto&& period : HoldingPeriods) {
        balance += period.balance;
    }
    
    return balance;
}

void Investor::UpdateGlobalBalance(const CWallet& wallet)
{
    LOCK(csInvestor);

    ResetBalance();

    for (auto&& txp : wallet.mapWallet) {
        for (auto&& period : HoldingPeriods) {
            for (auto&& output : txp.second.tx->vout) {
                if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress)) {
                    period.balance += output.nValue;
                }
            }

            for (auto&& input : txp.second.tx->vin) {
                CTxOut fundingTxOut;
                if (MultisigFundingOutputForInput(wallet, input, period.multisigAddress, fundingTxOut)) {
                    period.balance -= fundingTxOut.nValue;
                }
            }
        }

        AddTxHash(&txp.second);
    }
}

// investments tracking

bool Investor::TransactionIsMyInvestment(const CWalletTx* tx)
{
    assert(tx);
    if (!tx) { return false; }
    
    LOCK(csInvestor);
    
    for (auto&& output : tx->tx->vout) {
        for (auto&& period : HoldingPeriods) {
            if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress)) {
                return true;
            }
        }
    }

    return false;
}

bool Investor::TransactionIsUnlocked(const CWalletTx* tx)
{
    assert(tx);
    if (!tx) { return true; }
    
    LOCK(csInvestor);
    
    for (auto&& output : tx->tx->vout) {
        for (auto&& period : HoldingPeriods) {
            if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress) && (period.balance > 0)) {
                return false;
            }
        }
    }
    
    return true;
}

bool Investor::AddressIsMyMultisig(std::string& address)
{
    if (address.size() == 0) { return false; }
    
    LOCK(csInvestor);
    
    for (auto&& period : HoldingPeriods) {
        if (period.multisigAddress == address) {
            return true;
        }
    }

    return false;
}

bool Investor::Hash160IsMyMultisig(const uint160& hash160)
{
    if (hash160.size() == 0) { return false; }

    LOCK(csInvestor);

    std::vector<unsigned char> vaddr;

    std::vector<unsigned char> prefix = Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    vaddr.insert(vaddr.begin(), prefix.begin(), prefix.begin() + 1);

    vaddr.insert(vaddr.end(), hash160.begin(), hash160.end());

    std::string address = EncodeBase58Check(vaddr);

    for (auto&& period : HoldingPeriods) {
        if (period.multisigAddress == address) {
            return true;
        }
    }

    return false;
}

// expiration time

uint64_t Investor::SecondsUntilHoldingPeriodExpires(void)
{
    time_t t = time(NULL);
    if (t == ((time_t)-1)) { return 0; }
    
    uint64_t now = (uint64_t)t;
    
    LOCK(csInvestor);
    
    uint64_t seconds = 0;
    
    for (auto&& period : HoldingPeriods) {
        if ((period.balance > 0) && (now < period.expirationTimestamp)) {
            if ((seconds == 0) || ((period.expirationTimestamp - now) < seconds)) {
                seconds = (period.expirationTimestamp - now);
            }
        }
    }

    return seconds;
}

// unlock and update

bool Investor::ShouldUpdateApplication(const CWallet& wallet)
{
    // unlocking the investment has a higher priority,
    // so if this is available, then not need to update yet
    if (ShouldUnlockInvestment(wallet)) {
        return false;
    }
    
    time_t t = time(NULL);
    if (t == ((time_t)-1)) { return 0; }
    
    uint64_t now = (uint64_t)t;
    
    LOCK(csInvestor);
    
    // find an expired holding period with non-zero balance
    // that doesn't have the PAI private key yet and
    // for which the unlocking transaction has not been sent yet
    for (auto&& period : HoldingPeriods) {
        if ((period.balance > 0) && (now >= period.expirationTimestamp) && (!period.paiPrivateKey.IsValid())) {
            // detect the already spent based on actual address balance
            uint64_t sent = 0, received = 0;

            for (std::map<uint256, CWalletTx>::const_iterator txit = wallet.mapWallet.begin(); txit != wallet.mapWallet.end(); txit++) {
                const CWalletTx* tx = &(txit->second);

                for (auto&& input : tx->tx->vin) {
                    CTxOut fundingTxOut;
                    if (MultisigFundingOutputForInput(wallet, input, period.multisigAddress, fundingTxOut)) {
                        sent += fundingTxOut.nValue;
                    }
                }

                for (auto&& output : tx->tx->vout) {
                    if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress)) {
                        received += output.nValue;
                    }
                }
            }

            if ((received - sent) > 0) {
                return true;
            }
        }
    }
    
    return false;
}

bool Investor::ShouldUnlockInvestment(const CWallet& wallet)
{
    time_t t = time(NULL);
    if (t == ((time_t)-1)) { return 0; }

    uint64_t now = (uint64_t)t;

    LOCK(csInvestor);

    // find an expired holding period with non-zero balance
    // that does have the PAI private key yet and
    // for which the unlocking transaction has not been sent yet
    for (auto&& period : HoldingPeriods) {
        if ((period.balance > 0) && (now >= period.expirationTimestamp) && period.paiPrivateKey.IsValid()) {
            // detect the already spent based on actual address balance
            uint64_t sent = 0, received = 0;

            for (std::map<uint256, CWalletTx>::const_iterator txit = wallet.mapWallet.begin(); txit != wallet.mapWallet.end(); txit++) {
                const CWalletTx* tx = &(txit->second);

                for (auto&& input : tx->tx->vin) {
                    CTxOut fundingTxOut;
                    if (MultisigFundingOutputForInput(wallet, input, period.multisigAddress, fundingTxOut)) {
                        sent += fundingTxOut.nValue;
                    }
                }

                for (auto&& output : tx->tx->vout) {
                    if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress)) {
                        received += output.nValue;
                    }
                }
            }

            if ((received - sent) > 0) {
                return true;
            }
        }
    }

    return false;
}

// spend funds

bool Investor::CreateUnlockTransaction(CWallet& wallet, const CKey& privateKey, const CPubKey& pubKey, CMutableTransaction& unlockTransaction)
{
    // validation
    if ((!privateKey.IsValid()) || (wallet.mapWallet.size() == 0) || (!pubKey.IsValid())) {
        return false;
    }
    
    LOCK(csInvestor);
    
    // initial values and standard settings
    unlockTransaction.nVersion = 2;
    unlockTransaction.nLockTime = 0;
    
    // process all the transaction and add the appropriate inputs for all the holding periods that have been unlocked, while also calculating the total input
    unlockTransaction.vin.clear();
    uint64_t totalAmount = 0;
    for (auto&& tx : wallet.mapWallet) {
        for (size_t i = 0; i < tx.second.tx->vout.size(); ++i) {
            // make sure the transaction's output is not already spent
            bool alreadySpent = false;
            for (auto&& astx : wallet.mapWallet) {
                for (auto&& asin : astx.second.tx->vin) {
                    if ((asin.prevout.hash == tx.second.tx->GetHash()) && (asin.prevout.n == i)) {
                        alreadySpent = true;
                        break;
                    }
                }

                if (alreadySpent) {
                    break;
                }
            }

            if (alreadySpent) {
                continue;
            }

            // find the corresponding holding period (if any), and create the input
            CTxOut output = tx.second.tx->vout[i];

            for (auto&& period : HoldingPeriods) {
                if (!period.paiPrivateKey.IsValid()) {
                    continue;
                }

                if (DoesScriptPayToMultisigAddress(output.scriptPubKey, period.multisigAddress)) {
                    COutPoint prevOut;
                    prevOut.hash = tx.second.tx->GetHash();
                    prevOut.n = i;

                    CTxIn input;
                    input.prevout = prevOut;
                    input.scriptSig = period.redeemScript;
                    input.nSequence = CTxIn::SEQUENCE_FINAL;

                    unlockTransaction.vin.push_back(input);

                    totalAmount += output.nValue;
                }
            }
        }
    }
    
    // if the transaction doesn't have inputs at this stage it means it was not successfully built
    if (unlockTransaction.vin.size() == 0) {
        return false;
    }
    
    // add the personal address output
    CTxOut output;
    output.nValue = totalAmount;
    output.scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubKey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

    unlockTransaction.vout.push_back(output);

    // duplicate the unlocking transaction and add the signatures in order to calculate the fee using the correct size
    CMutableTransaction unlockTransactionForSigning(unlockTransaction);
    SignUnlockTransaction(wallet, privateKey, unlockTransactionForSigning);

    // calculate the miner fee (based on the size round up to the nearest kb)
    uint64_t fee = GetRequiredFee(GetVirtualTransactionSize(unlockTransactionForSigning, 2));

    // adjust the output value to consider the fee and sign the original transaction
    unlockTransaction.vout[0].nValue = totalAmount - fee;
    SignUnlockTransaction(wallet, privateKey, unlockTransaction);

    //std::cout << "Unlock transaction: " << CTransaction(unlockTransaction).ToString();
    //std::cout.flush();

    //std::cout << "Unlock transaction hex: " << EncodeHexTx(unlockTransaction);
    //std::cout.flush();
    
    return true;
}

void Investor::SignUnlockTransaction(CWallet& wallet, const CKey& privateKey, CMutableTransaction& unlockTransaction) {
    // sign all inputs (maintain all the signature separated from the transaction until finishing all signatures)
    std::vector<CScript> sigScript;
    for (size_t i = 0; i < unlockTransaction.vin.size(); i++) {
        CTxIn *input = &unlockTransaction.vin[i];

        // find PAI private key
        CKey paiPrivateKey;
        for (auto&& period : HoldingPeriods) {
            if (!period.paiPrivateKey.IsValid()) {
                continue;
            }

            if (input->scriptSig == period.redeemScript) {
                paiPrivateKey = period.paiPrivateKey;
                break;
            }
        }

        if (!paiPrivateKey.IsValid()) {
            continue;
        }

        // create the message hash to be signed
        uint256 md = SignatureHash(input->scriptSig, unlockTransaction, i, SIGHASH_ALL, 0 /* not necessary for SIGVERSION_BASE */, SIGVERSION_BASE);
        if (md.IsNull()) {
            continue;
        }

        // sign by the investor
        std::vector<unsigned char> sigInvestor;
        if (!privateKey.Sign(md, sigInvestor)) {
            continue;
        }
        sigInvestor.push_back(SIGHASH_ALL);

        // sign by PAI
        std::vector<unsigned char> sigPai;
        if (!paiPrivateKey.Sign(md, sigPai)) {
            continue;
        }
        sigPai.push_back(SIGHASH_ALL);

        // build signature script
        CScript script;

        script << OP_0;

        script << sigInvestor;

        script << sigPai;

        script << std::vector<unsigned char>(input->scriptSig.begin(), input->scriptSig.end());

        sigScript.push_back(script);

        script.clear();
    }

    // store the signatures
    for (size_t i = 0; i < unlockTransaction.vin.size(); i++) {
        unlockTransaction.vin[i].scriptSig = sigScript[i];
        sigScript[i].clear();
    }
}

bool Investor::IsUnlockTransaction(const CWallet& wallet, const CWalletTx* tx)
{
    assert(tx);
    if (!tx) { return false; }

    LOCK(csInvestor);

    for (auto&& input : tx->tx->vin) {
        bool isMyMultisig = false;

        for (auto&& period : HoldingPeriods) {
            CTxOut fundingTxOut;
            if (MultisigFundingOutputForInput(wallet, input, period.multisigAddress, fundingTxOut)) {
                isMyMultisig = true;
            }
        }

        if (!isMyMultisig) {
            return false;
        }
    }

    return true;
}

// wipe

void Investor::ResetBalance(void)
{
    LOCK(csInvestor);

    for (auto&& period : HoldingPeriods) {
        period.balance = 0;
    }

    CleanTxHashList();
}

void Investor::Wipe(void)
{
    LOCK(csInvestor);
    
    for (auto&& period : HoldingPeriods) {
        period.investorPublicKey = CPubKey();
        period.multisigAddress = "";
        period.redeemScript = CScript();
        period.balance = 0;
    }
    
    ResetBalance();
}
