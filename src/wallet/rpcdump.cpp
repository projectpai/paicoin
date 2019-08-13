// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include "chain.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "init.h"
#include "validation.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet.h"
#include "merkleblock.h"
#include "core_io.h"

#include "rpcwallet.h"

#include <fstream>
#include <iterator>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>


std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string &str) {
    static const auto epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc{std::locale::classic(),
        new boost::posix_time::time_input_facet{"%Y-%m-%dT%H:%M:%SZ"}};
    std::istringstream iss{str};
    iss.imbue(loc);
    boost::posix_time::ptime ptime{boost::date_time::not_a_date_time};
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (std::string::size_type pos{0}; pos < str.length(); ++pos) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) |
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

UniValue importprivkey(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error{
            "importprivkey \"privkey\" ( \"label\" ) ( rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"privkey\"          (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nImport using default blank label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    const auto& strSecret = request.params[0].get_str();
    std::string strLabel;
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    auto fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Rescan is disabled in pruned mode");

    CKey key = DecodeSecret(strSecret);
    if (!key.IsValid()) throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    const auto pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    const auto vchAddress = pubkey.GetID();
    {
        pwallet->MarkDirty();
        pwallet->SetAddressBook(vchAddress, strLabel, "receive");

        // Don't throw error in case a key is already there
        if (pwallet->HaveKey(vchAddress)) {
            return NullUniValue;
        }

        pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwallet->AddKeyPubKey(key, pubkey)) {
            throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding key to wallet");
        }

        // whenever a key is imported, we need to scan the whole chain
        pwallet->UpdateTimeFirstKey(1);

        if (fRescan) {
            pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        }
    }

    return NullUniValue;
}

UniValue abortrescan(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error{
            "abortrescan\n"
            "\nStops current wallet rescan triggered e.g. by an importprivkey call.\n"
            "\nExamples:\n"
            "\nImport a private key\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nAbort the running wallet rescan\n"
            + HelpExampleCli("abortrescan", "") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("abortrescan", "")
        };

    ObserveSafeMode();
    if (!pwallet->IsScanning() || pwallet->IsAbortingRescan()) return false;
    pwallet->AbortRescan();
    return true;
}

static void ImportAddress(CWallet*, const CTxDestination& dest, const std::string& strLabel);
static void ImportScript(CWallet* const pwallet, const CScript& script, const std::string& strLabel, bool isRedeemScript)
{
    if (!isRedeemScript && ::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
    }

    pwallet->MarkDirty();

    if (!pwallet->HaveWatchOnly(script) && !pwallet->AddWatchOnly(script, 0 /* nCreateTime */)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
    }

    if (isRedeemScript) {
        if (!pwallet->HaveCScript(script) && !pwallet->AddCScript(script)) {
            throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
        }
        ImportAddress(pwallet, CScriptID(script), strLabel);
    } else {
        CTxDestination destination;
        if (ExtractDestination(script, destination)) {
            pwallet->SetAddressBook(destination, strLabel, "receive");
        }
    }
}

static void ImportAddress(CWallet* const pwallet, const CTxDestination& dest, const std::string& strLabel)
{
    const auto script = GetScriptForDestination(dest);
    ImportScript(pwallet, script, strLabel, false);
    // add to address book or update label
    if (IsValidDestination(dest))
        pwallet->SetAddressBook(dest, strLabel, "receive");
}

UniValue importaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error{
            "importaddress \"address\" ( \"label\" rescan p2sh )\n"
            "\nAdds a script (in hex) or address that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"script\"           (string, required) The hex-encoded script (or address)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "4. p2sh                 (boolean, optional, default=false) Add the P2SH version of the script as well\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "If you have the full public key, you should call importpubkey instead of this.\n"
            "\nNote: If you import a non-standard raw script in hex form, outputs sending to it will be treated\n"
            "as change, and not show up in many RPCs.\n"
            "\nExamples:\n"
            "\nImport a script with rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importaddress", "\"myscript\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importaddress", "\"myscript\", \"testing\", false")
        };

    std::string strLabel;
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    auto fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Rescan is disabled in pruned mode");

    // Whether to import a p2sh version, too
    auto fP2SH = false;
    if (!request.params[3].isNull())
        fP2SH = request.params[3].get_bool();

    LOCK2(cs_main, pwallet->cs_wallet);

    const auto dest = DecodeDestination(request.params[0].get_str());
    if (IsValidDestination(dest)) {
        if (fP2SH) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");
        }
        ImportAddress(pwallet, dest, strLabel);
    } else if (IsHex(request.params[0].get_str())) {
        const auto data = ParseHex(request.params[0].get_str());
        ImportScript(pwallet, CScript(std::begin(data), std::end(data)), strLabel, fP2SH);
    } else {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address or script");
    }

    if (fRescan)
    {
        pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importprunedfunds(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "importprunedfunds\n"
            "\nImports funds without rescan. Corresponding address or script must previously be included in wallet. Aimed towards pruned wallets. The end-user is responsible to import additional transactions that subsequently spend the imported outputs or rescan after the point in the blockchain the transaction is included.\n"
            "\nArguments:\n"
            "1. \"rawtransaction\" (string, required) A raw transaction in hex funding an already-existing address in wallet\n"
            "2. \"txoutproof\"     (string, required) The hex output from gettxoutproof that contains the transaction\n"
        };

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str()))
        throw JSONRPCError(RPCErrorCode::DESERIALIZATION_ERROR, "TX decode failed");
    const auto hashTx = tx.GetHash();
    CWalletTx wtx{pwallet, MakeTransactionRef(std::move(tx))};

    CDataStream ssMB{ParseHexV(request.params[1], "proof"), SER_NETWORK, PROTOCOL_VERSION};
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    //Search partial merkle tree in proof for our transaction and index in valid block
    std::vector<uint256> vMatch;
    std::vector<unsigned int> vIndex;
    unsigned int txnIndex{0};
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) == merkleBlock.header.hashMerkleRoot) {

        LOCK(cs_main);

        if (!mapBlockIndex.count(merkleBlock.header.GetHash()) || !chainActive.Contains(mapBlockIndex[merkleBlock.header.GetHash()]))
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found in chain");

        auto it = std::find(vMatch.begin(), vMatch.end(), hashTx);
        if (it == vMatch.end()) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Transaction given doesn't exist in proof");
        }

        txnIndex = vIndex[it - vMatch.begin()];
    }
    else {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Something wrong with merkleblock");
    }

    wtx.nIndex = txnIndex;
    wtx.hashBlock = merkleBlock.header.GetHash();

    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->IsMine(*wtx.tx)) {
        pwallet->AddToWallet(wtx, false);
        return NullUniValue;
    }

    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "No addresses in wallet correspond to included transaction");
}

UniValue removeprunedfunds(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "removeprunedfunds \"txid\"\n"
            "\nDeletes the specified transaction from the wallet. Meant for use with pruned wallets and as a companion to importprunedfunds. This will affect wallet balances.\n"
            "\nArguments:\n"
            "1. \"txid\"           (string, required) The hex-encoded id of the transaction you are deleting\n"
            "\nExamples:\n"
            + HelpExampleCli("removeprunedfunds", "\"a8d0c0184dde994a09ec054286f1ce581bebf46446a512166eae7628734ea0a5\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("removeprunedfunds", "\"a8d0c0184dde994a09ec054286f1ce581bebf46446a512166eae7628734ea0a5\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());
    std::vector<uint256> vHash;
    vHash.push_back(hash);
    std::vector<uint256> vHashOut;

    if (pwallet->ZapSelectTx(vHash, vHashOut) != DB_LOAD_OK) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Could not properly delete the transaction.");
    }

    if(vHashOut.empty()) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Transaction does not exist in wallet.");
    }

    return NullUniValue;
}

UniValue importpubkey(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error{
            "importpubkey \"pubkey\" ( \"label\" rescan )\n"
            "\nAdds a public key (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"pubkey\"           (string, required) The hex-encoded public key\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nImport a public key with rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false")
        };

    std::string strLabel;
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    // Whether to perform rescan after import
    auto fRescan = true;
    if (!request.params[2].isNull())
        fRescan = request.params[2].get_bool();

    if (fRescan && fPruneMode)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Rescan is disabled in pruned mode");

    if (!IsHex(request.params[0].get_str()))
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");

    const auto data = ParseHex(request.params[0].get_str());
    CPubKey pubKey{std::begin(data), std::end(data)};
    if (!pubKey.IsFullyValid())
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");

    LOCK2(cs_main, pwallet->cs_wallet);

    ImportAddress(pwallet, pubKey.GetID(), strLabel);
    ImportScript(pwallet, GetScriptForRawPubKey(pubKey), strLabel, false);

    if (fRescan)
    {
        pwallet->RescanFromTime(TIMESTAMP_MIN, true /* update */);
        pwallet->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importwallet(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "importwallet \"filename\"\n"
            "\nImports keys from a wallet dump file (see dumpwallet).\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet file\n"
            "\nExamples:\n"
            "\nDump the wallet\n"
            + HelpExampleCli("dumpwallet", "\"test\"") +
            "\nImport the wallet\n"
            + HelpExampleCli("importwallet", "\"test\"") +
            "\nImport using the json rpc call\n"
            + HelpExampleRpc("importwallet", "\"test\"")
        };

    if (fPruneMode)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Importing wallets is disabled in pruned mode");

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::ifstream file;
    file.open(request.params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Cannot open wallet dump file");

    auto nTimeBegin = chainActive.Tip()->GetBlockTime();

    auto fGood = true;

    const auto nFilesize = std::max<int64_t>(1, file.tellg());
    file.seekg(0, file.beg);

    pwallet->ShowProgress(_("Importing..."), 0); // show progress dialog in GUI
    while (file.good()) {
        pwallet->ShowProgress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nFilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        CKey key = DecodeSecret(vstr[0]);
        if (!key.IsValid())
            continue;

        const auto pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        const auto keyid = pubkey.GetID();
        if (pwallet->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)\n", EncodeDestination(keyid));
            continue;
        }
        const auto nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        auto fLabel = true;
        for (auto nStr = 2u; nStr < vstr.size(); ++nStr) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...\n", EncodeDestination(keyid));
        if (!pwallet->AddKeyPubKey(key, pubkey)) {
            fGood = false;
            continue;
        }
        pwallet->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwallet->SetAddressBook(keyid, strLabel, "receive");
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();
    pwallet->ShowProgress("", 100); // hide progress dialog in GUI
    pwallet->UpdateTimeFirstKey(nTimeBegin);
    pwallet->RescanFromTime(nTimeBegin, false /* update */);
    pwallet->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding some keys to wallet");

    return NullUniValue;
}

UniValue dumpprivkey(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "dumpprivkey \"address\"\n"
            "\nReveals the private key corresponding to 'address'.\n"
            "Then the importprivkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"address\"   (string, required) The paicoin address for the private key\n"
            "\nResult:\n"
            "\"key\"                (string) The private key\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"")
            + HelpExampleCli("importprivkey", "\"mykey\"")
            + HelpExampleRpc("dumpprivkey", "\"myaddress\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    const auto& strAddress = request.params[0].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }
    const auto * const keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    if (!pwallet->GetKey(*keyID, vchSecret)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    }
    return EncodeSecret(vchSecret);
}

UniValue dumpwallet(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "dumpwallet \"filename\"\n"
            "\nDumps all wallet keys in a human-readable format to a server-side file. This does not allow overwriting existing files.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename with path (either absolute or relative to paicoind)\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"filename\" : {        (string) The filename with full absolute path\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpwallet", "\"test\"")
            + HelpExampleRpc("dumpwallet", "\"test\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    boost::filesystem::path filepath = request.params[0].get_str();
    filepath = boost::filesystem::absolute(filepath);

    /* Prevent arbitrary files from being overwritten. There have been reports
     * that users have overwritten wallet files this way:
     * https://github.com/bitcoin/bitcoin/issues/9934
     * It may also avoid other security issues.
     */
    if (boost::filesystem::exists(filepath)) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, filepath.string() + " already exists. If you are sure this is what you want, move it out of the way first");
    }

    std::ofstream file;
    file.open(filepath.string().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CTxDestination, int64_t> mapKeyBirth;
    const auto& mapKeyPool = pwallet->GetAllReserveKeys();
    pwallet->GetKeyBirthTimes(mapKeyBirth);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (const auto& entry : mapKeyBirth) {
        if (const CKeyID* keyID = boost::get<CKeyID>(&entry.first)) { // set and test
            vKeyBirth.push_back(std::make_pair(entry.second, *keyID));
        }
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by PAIcoin %s\n", CLIENT_BUILD);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    const auto masterKeyID = pwallet->GetHDChain().masterKeyID;
    if (!masterKeyID.IsNull())
    {
        CKey key;
        if (pwallet->GetKey(masterKeyID, key)) {
            CExtKey masterKey;
            masterKey.SetMaster(key.begin(), key.size());

            file << "# extended private masterkey: " << EncodeExtKey(masterKey) << "\n\n";
        }
    }

    for (const auto& it : vKeyBirth) {
        const auto& keyid = it.second;
        const auto strTime = EncodeDumpTime(it.first);
        const auto strAddr = EncodeDestination(keyid);
        CKey key;
        if (pwallet->GetKey(keyid, key)) {
            file << strprintf("%s %s ", EncodeSecret(key), strTime);
            if (pwallet->mapAddressBook.count(keyid)) {
                file << strprintf("label=%s", EncodeDumpString(pwallet->mapAddressBook[keyid].name));
            } else if (keyid == masterKeyID) {
                file << "hdmaster=1";
            } else if (mapKeyPool.count(keyid)) {
                file << "reserve=1";
            } else if (pwallet->mapKeyMetadata[keyid].hdKeypath == "m") {
                file << "inactivehdmaster=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwallet->mapKeyMetadata[keyid].hdKeypath.size() > 0 ? " hdkeypath="+pwallet->mapKeyMetadata[keyid].hdKeypath : ""));
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    UniValue reply{UniValue::VOBJ};
    reply.push_back(Pair("filename", filepath.string()));

    return reply;
}

UniValue ProcessImport(CWallet * const pwallet, const UniValue& data, const int64_t timestamp)
{
    try {
        auto success = false;

        // Required fields.
        const UniValue& scriptPubKey = data["scriptPubKey"];

        // Should have script or JSON with "address".
        if (!(scriptPubKey.getType() == UniValue::VOBJ && scriptPubKey.exists("address")) && !(scriptPubKey.getType() == UniValue::VSTR)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid scriptPubKey");
        }

        // Optional fields.
        const auto& strRedeemScript = data.exists("redeemscript") ? data["redeemscript"].get_str() : "";
        const auto& pubKeys = data.exists("pubkeys") ? data["pubkeys"].get_array() : UniValue{};
        const auto& keys = data.exists("keys") ? data["keys"].get_array() : UniValue{};
        const auto internal = data.exists("internal") ? data["internal"].get_bool() : false;
        const auto watchOnly = data.exists("watchonly") ? data["watchonly"].get_bool() : false;
        const auto& label = data.exists("label") && !internal ? data["label"].get_str() : "";

        const auto isScript = scriptPubKey.getType() == UniValue::VSTR;
        const auto isP2SH = strRedeemScript.length() > 0;
        const auto& output = isScript ? scriptPubKey.get_str() : scriptPubKey["address"].get_str();

        // Parse the output.
        CScript script;
        CTxDestination dest;

        if (!isScript) {
            dest = DecodeDestination(output);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid address");
            }
            script = GetScriptForDestination(dest);
        } else {
            if (!IsHex(output)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid scriptPubKey");
            }

            const auto vData = ParseHex(output);
            script = CScript(vData.begin(), vData.end());
        }

        // Watchonly and private keys
        if (watchOnly && keys.size()) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Incompatibility found between watchonly and keys");
        }

        // Internal + Label
        if (internal && data.exists("label")) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Incompatibility found between internal and label");
        }

        // Not having Internal + Script
        if (!internal && isScript) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Internal must be set for hex scriptPubKey");
        }

        // Keys / PubKeys size check.
        if (!isP2SH && (keys.size() > 1 || pubKeys.size() > 1)) { // Address / scriptPubKey
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "More than private key given for one address");
        }

        // Invalid P2SH redeemScript
        if (isP2SH && !IsHex(strRedeemScript)) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid redeem script");
        }

        // Process. //

        // P2SH
        if (isP2SH) {
            // Import redeem script.
            const auto vData = ParseHex(strRedeemScript);
            const auto redeemScript = CScript{vData.begin(), vData.end()};

            // Invalid P2SH address
            if (!script.IsPayToScriptHash()) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid P2SH address / script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(redeemScript, timestamp)) {
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
            }

            if (!pwallet->HaveCScript(redeemScript) && !pwallet->AddCScript(redeemScript)) {
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
            }

            const CTxDestination redeemDest{CScriptID{redeemScript}};
            const auto redeemDestinationScript = GetScriptForDestination(redeemDest);

            if (::IsMine(*pwallet, redeemDestinationScript) == ISMINE_SPENDABLE) {
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(redeemDestinationScript, timestamp)) {
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
            }

            // add to address book or update label
            if (IsValidDestination(dest)) {
                pwallet->SetAddressBook(dest, label, "receive");
            }

            // Import private keys.
            if (keys.size()) {
                for (size_t i{0}; i < keys.size(); ++i) {
                    const auto& privkey = keys[i].get_str();

                    CKey key = DecodeSecret(privkey);

                    if (!key.IsValid()) {
                        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
                    }

                    const auto pubkey = key.GetPubKey();
                    assert(key.VerifyPubKey(pubkey));

                    const auto vchAddress = pubkey.GetID();
                    pwallet->MarkDirty();
                    pwallet->SetAddressBook(vchAddress, label, "receive");

                    if (pwallet->HaveKey(vchAddress)) {
                        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Already have this key");
                    }

                    pwallet->mapKeyMetadata[vchAddress].nCreateTime = timestamp;

                    if (!pwallet->AddKeyPubKey(key, pubkey)) {
                        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding key to wallet");
                    }

                    pwallet->UpdateTimeFirstKey(timestamp);
                }
            }

            success = true;
        } else {
            // Import public keys.
            if (pubKeys.size() && keys.size() == 0) {
                const auto& strPubKey = pubKeys[0].get_str();

                if (!IsHex(strPubKey)) {
                    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
                }

                const auto vData = ParseHex(strPubKey);
                const CPubKey pubKey{vData.begin(), vData.end()};

                if (!pubKey.IsFullyValid()) {
                    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");
                }

                const CTxDestination pubkeyDest{pubKey.GetID()};

                // Consistency check.
                if (!isScript && !(pubkeyDest == dest)) {
                    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                }

                // Consistency check.
                if (isScript) {
                    CTxDestination destination;

                    if (ExtractDestination(script, destination)) {
                        if (!(destination == pubkeyDest)) {
                            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                        }
                    }
                }

                const auto pubKeyScript = GetScriptForDestination(pubkeyDest);

                if (::IsMine(*pwallet, pubKeyScript) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(pubKeyScript, timestamp)) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
                }

                // add to address book or update label
                if (IsValidDestination(pubkeyDest)) {
                    pwallet->SetAddressBook(pubkeyDest, label, "receive");
                }

                // TODO Is this necessary?
                const auto scriptRawPubKey = GetScriptForRawPubKey(pubKey);

                if (::IsMine(*pwallet, scriptRawPubKey) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(scriptRawPubKey, timestamp)) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
                }

                success = true;
            }

            // Import private keys.
            if (keys.size()) {
                const auto& strPrivkey = keys[0].get_str();

                // Checks.
                CKey key = DecodeSecret(strPrivkey);

                if (!key.IsValid()) {
                    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
                }

                const auto pubKey = key.GetPubKey();
                assert(key.VerifyPubKey(pubKey));

                const CTxDestination pubkeyDest{pubKey.GetID()};

                // Consistency check.
                if (!isScript && !(pubkeyDest == dest)) {
                    throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                }

                // Consistency check.
                if (isScript) {
                    CTxDestination destination;

                    if (ExtractDestination(script, destination)) {
                        if (!(destination == pubkeyDest)) {
                            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Consistency check failed");
                        }
                    }
                }

                const auto vchAddress = pubKey.GetID();
                pwallet->MarkDirty();
                pwallet->SetAddressBook(vchAddress, label, "receive");

                if (pwallet->HaveKey(vchAddress)) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->mapKeyMetadata[vchAddress].nCreateTime = timestamp;

                if (!pwallet->AddKeyPubKey(key, pubKey)) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding key to wallet");
                }

                pwallet->UpdateTimeFirstKey(timestamp);

                success = true;
            }

            // Import scriptPubKey only.
            if (pubKeys.size() == 0 && keys.size() == 0) {
                if (::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "The wallet already contains the private key for this address or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(script, timestamp)) {
                    throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error adding address to wallet");
                }

                if (scriptPubKey.getType() == UniValue::VOBJ) {
                    // add to address book or update label
                    if (IsValidDestination(dest)) {
                        pwallet->SetAddressBook(dest, label, "receive");
                    }
                }

                success = true;
            }
        }

        auto result = UniValue{UniValue::VOBJ};
        result.pushKV("success", UniValue(success));
        return result;
    } catch (const UniValue& e) {
        auto result = UniValue{UniValue::VOBJ};
        result.pushKV("success", UniValue(false));
        result.pushKV("error", e);
        return result;
    } catch (...) {
        auto result = UniValue{UniValue::VOBJ};
        result.pushKV("success", UniValue(false));
        result.pushKV("error", JSONRPCError(RPCErrorCode::MISC_ERROR, "Missing required fields"));
        return result;
    }
}

static int64_t GetImportTimestamp(const UniValue& data, int64_t now)
{
    if (data.exists("timestamp")) {
        const auto& timestamp = data["timestamp"];
        if (timestamp.isNum()) {
            return timestamp.get_int64();
        } else if (timestamp.isStr() && timestamp.get_str() == "now") {
            return now;
        }
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, strprintf("Expected number or \"now\" timestamp value for key. got type %s", uvTypeName(timestamp.type())));
    }
    throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Missing required timestamp field for key");
}

UniValue importmulti(const JSONRPCRequest& mainRequest)
{
    const auto pwallet = GetWalletForJSONRPCRequest(mainRequest);
    if (!EnsureWalletIsAvailable(pwallet, mainRequest.fHelp)) {
        return NullUniValue;
    }

    // clang-format off
    if (mainRequest.fHelp || mainRequest.params.size() < 1 || mainRequest.params.size() > 2)
        throw std::runtime_error{
            "importmulti \"requests\" ( \"options\" )\n\n"
            "Import addresses/scripts (with private or public keys, redeem script (P2SH)), rescanning all addresses in one-shot-only (rescan can be disabled via options).\n\n"
            "Arguments:\n"
            "1. requests     (array, required) Data to be imported\n"
            "  [     (array of json objects)\n"
            "    {\n"
            "      \"scriptPubKey\": \"<script>\" | { \"address\":\"<address>\" }, (string / json, required) Type of scriptPubKey (string for script, json for address)\n"
            "      \"timestamp\": timestamp | \"now\"                        , (integer / string, required) Creation time of the key in seconds since epoch (Jan 1 1970 GMT),\n"
            "                                                              or the string \"now\" to substitute the current synced blockchain time. The timestamp of the oldest\n"
            "                                                              key will determine how far back blockchain rescans need to begin for missing wallet transactions.\n"
            "                                                              \"now\" can be specified to bypass scanning, for keys which are known to never have been used, and\n"
            "                                                              0 can be specified to scan the entire blockchain. Blocks up to 2 hours before the earliest key\n"
            "                                                              creation time of all keys being imported by the importmulti call will be scanned.\n"
            "      \"redeemscript\": \"<script>\"                            , (string, optional) Allowed only if the scriptPubKey is a P2SH address or a P2SH scriptPubKey\n"
            "      \"pubkeys\": [\"<pubKey>\", ... ]                         , (array, optional) Array of strings giving pubkeys that must occur in the output or redeemscript\n"
            "      \"keys\": [\"<key>\", ... ]                               , (array, optional) Array of strings giving private keys whose corresponding public keys must occur in the output or redeemscript\n"
            "      \"internal\": <true>                                    , (boolean, optional, default: false) Stating whether matching outputs should be treated as not incoming payments\n"
            "      \"watchonly\": <true>                                   , (boolean, optional, default: false) Stating whether matching outputs should be considered watched even when they're not spendable, only allowed if keys are empty\n"
            "      \"label\": <label>                                      , (string, optional, default: '') Label to assign to the address (aka account name, for now), only allowed with internal=false\n"
            "    }\n"
            "  ,...\n"
            "  ]\n"
            "2. options                 (json, optional)\n"
            "  {\n"
            "     \"rescan\": <false>,         (boolean, optional, default: true) Stating if should rescan the blockchain after all imports\n"
            "  }\n"
            "\nExamples:\n" +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }, "
                                          "{ \"scriptPubKey\": { \"address\": \"<my 2nd address>\" }, \"label\": \"example 2\", \"timestamp\": 1455191480 }]'") +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }]' '{ \"rescan\": false}'") +

            "\nResponse is an array with the same size as the input that has the execution result :\n"
            "  [{ \"success\": true } , { \"success\": false, \"error\": { \"code\": -1, \"message\": \"Internal Server Error\"} }, ... ]\n"
        };

    // clang-format on

    RPCTypeCheck(mainRequest.params, {UniValue::VARR, UniValue::VOBJ});

    const auto& requests = mainRequest.params[0];

    //Default options
    auto fRescan = true;

    if (!mainRequest.params[1].isNull()) {
        const auto& options = mainRequest.params[1];

        if (options.exists("rescan")) {
            fRescan = options["rescan"].get_bool();
        }
    }

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    // Verify all timestamps are present before importing any keys.
    const int64_t now{chainActive.Tip() ? chainActive.Tip()->GetMedianTimePast() : 0};
    for (const auto& data : requests.getValues()) {
        GetImportTimestamp(data, now);
    }

    auto fRunScan = false;
    const int64_t minimumTimestamp{1};
    int64_t nLowestTimestamp{0};

    if (fRescan && chainActive.Tip()) {
        nLowestTimestamp = chainActive.Tip()->GetBlockTime();
    } else {
        fRescan = false;
    }

    UniValue response{UniValue::VARR};

    for (const auto& data : requests.getValues()) {
        const auto timestamp = std::max(GetImportTimestamp(data, now), minimumTimestamp);
        const auto result = ProcessImport(pwallet, data, timestamp);
        response.push_back(result);

        if (!fRescan) {
            continue;
        }

        // If at least one request was successful then allow rescan.
        if (result["success"].get_bool()) {
            fRunScan = true;
        }

        // Get the lowest timestamp.
        if (timestamp < nLowestTimestamp) {
            nLowestTimestamp = timestamp;
        }
    }

    if (fRescan && fRunScan && requests.size()) {
        const auto scannedTime = pwallet->RescanFromTime(nLowestTimestamp, true /* update */);
        pwallet->ReacceptWalletTransactions();

        if (scannedTime > nLowestTimestamp) {
            const auto results = response.getValues();
            response.clear();
            response.setArray();
            size_t i{0};
            for (const auto& request : requests.getValues()) {
                // If key creation date is within the successfully scanned
                // range, or if the import result already has an error set, let
                // the result stand unmodified. Otherwise replace the result
                // with an error message.
                if (scannedTime <= GetImportTimestamp(request, now) || results.at(i).exists("error")) {
                    response.push_back(results.at(i));
                } else {
                    auto result = UniValue{UniValue::VOBJ};
                    result.pushKV("success", UniValue(false));
                    result.pushKV(
                        "error",
                        JSONRPCError(
                            RPCErrorCode::MISC_ERROR,
                            strprintf("Rescan failed for key with creation timestamp %d. There was an error reading a "
                                      "block from time %d, which is after or within %d seconds of key creation, and "
                                      "could contain transactions pertaining to the key. As a result, transactions "
                                      "and coins using this key may not appear in the wallet. This error could be "
                                      "caused by pruning or data corruption (see paicoind log for details) and could "
                                      "be dealt with by downloading and rescanning the relevant blocks (see -reindex "
                                      "and -rescan options).",
                                GetImportTimestamp(request, now), scannedTime - TIMESTAMP_WINDOW - 1, TIMESTAMP_WINDOW)));
                    response.push_back(std::move(result));
                }
                ++i;
            }
        }
    }

    return response;
}

UniValue importscript(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error{
            "importscript \"script\" ( rescan scanfrom )\n"
            "\nImport a redeem script.\n"
            "\nArguments:\n"
            "1. \"script\"           (string, required)                             The hex-encoded script to import\n"
            "2. rescan               (boolean, optional, default=true)              Rescans the blockchain (since the genesis block, or scanfrom block) for outputs controlled by the imported key\n"
            "3. scanfrom             (numeric, optional, default=genesis block)     Block number for where to start rescan from\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            + HelpExampleCli("importscript", "\"myscript\" true 5") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importscript", "\"myscript\" true 5")
        };

    auto script = request.params[0].get_str();
    if (script.empty()) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Script cannot be empty");
    }
    if (!IsHex(script)) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMS, "Script is not correctly hex-encoded");
    }

    // Whether to perform rescan after import
    auto rescan = true;
    if (!request.params[1].isNull())
        rescan = request.params[1].get_bool();
    if (rescan && fPruneMode)
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Rescan is disabled in pruned mode");
    
    int scanfrom = chainActive.Genesis()->nHeight;
    if (rescan && !request.params[2].isNull()) {
        scanfrom = request.params[2].get_int();
        if (scanfrom < chainActive.Genesis()->nHeight || scanfrom > chainActive.Tip()->nHeight) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "The scanfrom parameter should be a valid block height");
        }
    }

    {
        LOCK2(cs_main, pwallet->cs_wallet);

        const auto data = ParseHex(request.params[0].get_str());
        ImportScript(pwallet, CScript(std::begin(data), std::end(data)), "", true);
    }

    if (rescan)
    {
        UniValue startHeight{UniValue::VNUM};
        startHeight.setInt(scanfrom);

        UniValue params{UniValue::VARR};
        params.push_back(std::make_pair("start_height", startHeight));

        JSONRPCRequest rescanchain;
        rescanchain.params = params;
        rescanblockchain(rescanchain);
    }

    return NullUniValue;
}
