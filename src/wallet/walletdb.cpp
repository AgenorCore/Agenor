// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb.h"

#include "fs.h"

#include "base58.h"
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "txdb.h"
#include "util.h"
#include "utiltime.h"
#include "wallet/wallet.h"
#include <zage/deterministicmint.h>

#include <atomic>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <fstream>


static uint64_t nAccountingEntryNumber = 0;

static std::atomic<unsigned int> nWalletDBUpdateCounter;

//
// CWalletDB
//

bool CWalletDB::WriteName(const std::string& strAddress, const std::string& strName)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const std::string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("name"), strAddress));
}

bool CWalletDB::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::ErasePurpose(const std::string& strPurpose)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("purpose"), strPurpose));
}

bool CWalletDB::WriteTx(const CWalletTx& wtx)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("tx"), wtx.GetHash()), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
            keyMeta, false))
        return false;

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return Write(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey& vchPubKey,
    const std::vector<unsigned char>& vchCryptedSecret,
    const CKeyMetadata& keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
            keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false))
        return false;
    if (fEraseUnencryptedKey) {
        Erase(std::make_pair(std::string("key"), vchPubKey));
        Erase(std::make_pair(std::string("wkey"), vchPubKey));
    }
    return true;
}

bool CWalletDB::WriteSaplingZKey(const libzcash::SaplingIncomingViewingKey &ivk,
                                 const libzcash::SaplingExtendedSpendingKey &key,
                                 const CKeyMetadata &keyMeta)
{
    nWalletDBUpdateCounter++;

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    return Write(std::make_pair(std::string("sapzkey"), ivk), key, false);
}

bool CWalletDB::WriteSaplingPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr,
        const libzcash::SaplingIncomingViewingKey &ivk)
{
    nWalletDBUpdateCounter++;

    return Write(std::make_pair(std::string("sapzaddr"), addr), ivk, false);
}

bool CWalletDB::WriteCryptedSaplingZKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char>& vchCryptedSecret,
        const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdateCounter++;
    auto ivk = extfvk.fvk.in_viewing_key();

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("csapzkey"), ivk), std::make_pair(extfvk, vchCryptedSecret), false))
        return false;

    if (fEraseUnencryptedKey) {
        Erase(std::make_pair(std::string("sapzkey"), ivk));
    }
    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("cscript"), hash), *(const CScriptBase*)(&redeemScript), false);
}

bool CWalletDB::WriteWatchOnly(const CScript& dest)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)), '1');
}

bool CWalletDB::EraseWatchOnly(const CScript& dest)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("watchs"), *(const CScriptBase*)(&dest)));
}

bool CWalletDB::WriteMultiSig(const CScript& dest)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("multisig"), *(const CScriptBase*)(&dest)), '1');
}

bool CWalletDB::EraseMultiSig(const CScript& dest)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("multisig"), *(const CScriptBase*)(&dest)));
}

bool CWalletDB::WriteBestBlock(const CBlockLocator& locator)
{
    nWalletDBUpdateCounter++;
    Write(std::string("bestblock"), CBlockLocator()); // Write empty block locator so versions that require a merkle branch automatically rescan
    return Write(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::ReadBestBlock(CBlockLocator& locator)
{
    if (Read(std::string("bestblock"), locator) && !locator.vHave.empty()) return true;
    return Read(std::string("bestblock_nomerkle"), locator);
}

bool CWalletDB::WriteOrderPosNext(int64_t nOrderPosNext)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("orderposnext"), nOrderPosNext);
}

bool CWalletDB::WriteStakeSplitThreshold(const CAmount& nStakeSplitThreshold)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("stakeSplitThreshold"), nStakeSplitThreshold);
}

bool CWalletDB::WriteUseCustomFee(bool fUse)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("fUseCustomFee"), fUse);
}

bool CWalletDB::WriteCustomFeeValue(const CAmount& nFee)
{
    nWalletDBUpdateCounter++;
    return Write(std::string("nCustomFee"), nFee);
}

bool CWalletDB::WriteMultiSend(std::vector<std::pair<std::string, int> > vMultiSend)
{
    return false;
    /* disable multisend
    nWalletDBUpdateCounter++;
    bool ret = true;
    for (unsigned int i = 0; i < vMultiSend.size(); i++) {
        std::pair<std::string, int> pMultiSend;
        pMultiSend = vMultiSend[i];
        if (!Write(std::make_pair(std::string("multisend"), i), pMultiSend, true))
            ret = false;
    }
    return ret;
    */
}

bool CWalletDB::EraseMultiSend(std::vector<std::pair<std::string, int> > vMultiSend)
{
    return false;
    /* disable multisend
    nWalletDBUpdateCounter++;
    bool ret = true;
    for (unsigned int i = 0; i < vMultiSend.size(); i++) {
        std::pair<std::string, int> pMultiSend;
        pMultiSend = vMultiSend[i];
        if (!Erase(std::make_pair(std::string("multisend"), i)))
            ret = false;
    }
    return ret;
    */
}

bool CWalletDB::WriteMSettings(bool fMultiSendStake, bool fMultiSendMasternode, int nLastMultiSendHeight)
{
    return false;
    /* disable multisend
    nWalletDBUpdateCounter++;
    std::pair<bool, bool> enabledMS(fMultiSendStake, fMultiSendMasternode);
    std::pair<std::pair<bool, bool>, int> pSettings(enabledMS, nLastMultiSendHeight);

    return Write(std::string("msettingsv2"), pSettings, true);
    */
}

bool CWalletDB::WriteMSDisabledAddresses(std::vector<std::string> vDisabledAddresses)
{
    return false;
    /* disable multisend
    nWalletDBUpdateCounter++;
    bool ret = true;
    for (unsigned int i = 0; i < vDisabledAddresses.size(); i++) {
        if (!Write(std::make_pair(std::string("mdisabled"), i), vDisabledAddresses[i]))
            ret = false;
    }
    return ret;
    */
}

bool CWalletDB::EraseMSDisabledAddresses(std::vector<std::string> vDisabledAddresses)
{
    return false;
    /* disable multisend
    nWalletDBUpdateCounter++;
    bool ret = true;
    for (unsigned int i = 0; i < vDisabledAddresses.size(); i++) {
        if (!Erase(std::make_pair(std::string("mdisabled"), i)))
            ret = false;
    }
    return ret;
    */
}

bool CWalletDB::WriteAutoCombineSettings(bool fEnable, CAmount nCombineThreshold)
{
    nWalletDBUpdateCounter++;
    std::pair<bool, CAmount> pSettings;
    pSettings.first = fEnable;
    pSettings.second = nCombineThreshold;
    return Write(std::string("autocombinesettings"), pSettings, true);
}

bool CWalletDB::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("pool"), nPool), keypool);
}

bool CWalletDB::ErasePool(int64_t nPool)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("pool"), nPool));
}

bool CWalletDB::WriteMinVersion(int nVersion)
{
    return Write(std::string("minversion"), nVersion);
}

bool CWalletDB::WriteHDChain(const CHDChain& chain)
{
    nWalletDBUpdateCounter++;
    std::string key = std::string("hdchain");
    if (chain.chainType == HDChain::ChainCounterType::Sapling)
        key += std::string("_sap");
    return Write(key, chain);
}

bool CWalletDB::ReadAccount(const std::string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(std::make_pair(std::string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const std::string& strAccount, const CAccount& account)
{
    return Write(std::make_pair(std::string("acc"), strAccount), account);
}

bool CWalletDB::EraseAccount(const std::string& strAccount)
{
    return Erase(std::make_pair(std::string("acc"), strAccount));
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(std::make_pair(std::string("acentry"), std::make_pair(acentry.strAccount, nAccEntryNum)), acentry);
}

bool CWalletDB::WriteAccountingEntry_Backend(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

CAmount CWalletDB::GetAccountCreditDebit(const std::string& strAccount)
{
    std::list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    CAmount nCreditDebit = 0;
    for (const CAccountingEntry& entry : entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    while (true) {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << std::make_pair(std::string("acentry"), std::make_pair((fAllAccounts ? std::string("") : strAccount), uint64_t(0)));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0) {
            pcursor->close();
            throw std::runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }

    pcursor->close();
}

DBErrors CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair> TxItems;
    TxItems txByTime;

    for (std::map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    std::list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    for (CAccountingEntry& entry : acentries) {
        txByTime.insert(std::make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it) {
        CWalletTx* const pwtx = (*it).second.first;
        CAccountingEntry* const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1) {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pwtx) {
                if (!WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        } else {
            int64_t nOrderPosOff = 0;
            for (const int64_t& nOffsetStart : nOrderPosOffsets) {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx) {
                if (!WriteTx(*pwtx))
                    return DB_LOAD_FAIL;
            } else if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                return DB_LOAD_FAIL;
        }
    }
    WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

class CWalletScanState
{
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    unsigned int nZKeys;
    unsigned int nZKeyMeta;
    unsigned int nSapZAddrs;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState()
    {
        nKeys = nCKeys = nKeyMeta = nZKeys = nZKeyMeta = nSapZAddrs = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue, CWalletScanState& wss, std::string& strType, std::string& strErr)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].name;
        } else if (strType == "purpose") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[DecodeDestination(strAddress)].purpose;
        } else if (strType == "tx") {
            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx;
            ssValue >> wtx;
            if (wtx.GetHash() != hash)
                return false;

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703) {
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                        wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->LoadToWallet(wtx);
        } else if (strType == "acentry") {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;

            if (!wss.fAnyUnordered) {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        } else if (strType == "watchs") {
            CScript script;
            ssKey >> *(CScriptBase*)(&script);
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);

            // Watch-only addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->nTimeFirstKey = 1;
        } else if (strType == "key" || strType == "wkey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key") {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using EC operations as a checksum.
            // Newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try {
                ssValue >> hash;
            } catch (...) {
            }

            bool fSkipCheck = false;

            if (!hash.IsNull()) {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<unsigned char> vchKey;
                vchKey.reserve(vchPubKey.size() + pkey.size());
                vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                if (Hash(vchKey.begin(), vchKey.end()) != hash) {
                    strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                    return false;
                }

                fSkipCheck = true;
            }

            if (!key.Load(pkey, vchPubKey, fSkipCheck)) {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            if (!pwallet->LoadKey(key, vchPubKey)) {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        } else if (strType == "ckey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            std::vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        } else if (strType == "defaultkey") {
            // We don't want or need the default key, but if there is one set,
            // we want to make sure that it is valid so that we can detect corruption
            CPubKey vchPubKey;
            ssValue >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: Default Key corrupt";
                return false;
            }
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;
            pwallet->GetScriptPubKeyMan()->LoadKeyPool(nIndex, keypool);
        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> *(CScriptBase*)(&script);
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        } else if (strType == "stakeSplitThreshold") {
            ssValue >> pwallet->nStakeSplitThreshold;
            // originally saved as integer
            if (pwallet->nStakeSplitThreshold < COIN)
                pwallet->nStakeSplitThreshold *= COIN;
        } else if (strType == "fUseCustomFee") {
            ssValue >> pwallet->fUseCustomFee;
        } else if (strType == "nCustomFee") {
            ssValue >> pwallet->nCustomFee;
        /* disable multisend
        } else if (strType == "multisend") {
            unsigned int i;
            ssKey >> i;
            std::pair<std::string, int> pMultiSend;
            ssValue >> pMultiSend;
            if (CBitcoinAddress(pMultiSend.first).IsValid()) {
                pwallet->vMultiSend.push_back(pMultiSend);
            }
        } else if (strType == "msettingsv2") {
            std::pair<std::pair<bool, bool>, int> pSettings;
            ssValue >> pSettings;
            pwallet->fMultiSendStake = pSettings.first.first;
            pwallet->fMultiSendMasternodeReward = pSettings.first.second;
            pwallet->nLastMultiSendHeight = pSettings.second;
        } else if (strType == "mdisabled") {
            std::string strDisabledAddress;
            ssValue >> strDisabledAddress;
            pwallet->vDisabledAddresses.push_back(strDisabledAddress);
            */
        } else if (strType == "autocombinesettings") {
            std::pair<bool, CAmount> pSettings;
            ssValue >> pSettings;
            pwallet->fCombineDust = pSettings.first;
            pwallet->nAutoCombineThreshold = pSettings.second;
        } else if (strType == "destdata") {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            if (!pwallet->LoadDestData(DecodeDestination(strAddress), strKey, strValue)) {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
        } else if (strType == "hdchain") { // Regular key chain counter
            CHDChain chain;
            ssValue >> chain;
            pwallet->GetScriptPubKeyMan()->SetHDChain(chain, true);
        } else if (strType == "hdchain_sap") {
            CHDChain chain;
            ssValue >> chain;
            pwallet->GetSaplingScriptPubKeyMan()->SetHDChain(chain, true);
        } else if (strType == "sapzkey") {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            libzcash::SaplingExtendedSpendingKey key;
            ssValue >> key;

            if (!pwallet->LoadSaplingZKey(key)) {
                strErr = "Error reading wallet database: LoadSaplingZKey failed";
                return false;
            }

            //add checks for integrity
            wss.nZKeys++;
        } else if (strType == "csapzkey") {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            libzcash::SaplingExtendedFullViewingKey extfvk;
            ssValue >> extfvk;
            std::vector<unsigned char> vchCryptedSecret;
            ssValue >> vchCryptedSecret;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedSaplingZKey(extfvk, vchCryptedSecret)) {
                strErr = "Error reading wallet database: LoadCryptedSaplingZKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "sapzkeymeta") {
            libzcash::SaplingIncomingViewingKey ivk;
            ssKey >> ivk;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;

            wss.nZKeyMeta++;

            pwallet->LoadSaplingZKeyMetadata(ivk, keyMeta);
        } else if (strType == "sapzaddr") {
            libzcash::SaplingPaymentAddress addr;
            ssKey >> addr;
            libzcash::SaplingIncomingViewingKey ivk;
            ssValue >> ivk;

            wss.nSapZAddrs++;

            if (!pwallet->LoadSaplingPaymentAddress(addr, ivk)) {
                strErr = "Error reading wallet database: LoadSaplingPaymentAddress failed";
                return false;
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

static bool IsKeyType(std::string strType)
{
    return (strType == "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey" ||
            strType == "sapzkey" || strType == "csapzkey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    LOCK(pwallet->cs_wallet);
    try {
        int nMinVersion = 0;
        if (Read((std::string) "minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr)) {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType) || strType == "defaultkey")
                    result = DB_CORRUPT;
                else {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")
                        // Rescan if there is a bad transaction record:
                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                LogPrintf("%s\n", strErr);
        }
        pcursor->close();
    } catch (const boost::thread_interrupted&) {
        throw;
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
        wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    LogPrintf("ZKeys: %u plaintext, -- encrypted, %u w/metadata, %u total\n",
              wss.nZKeys, wss.nZKeyMeta, wss.nZKeys + 0);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'

    for (uint256 hash : wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    pwallet->laccentries.clear();
    ListAccountCreditDebit("*", pwallet->laccentries);
    for (CAccountingEntry& entry : pwallet->laccentries) {
        pwallet->wtxOrdered.insert(std::make_pair(entry.nOrderPos, CWallet::TxPair((CWalletTx*)0, &entry)));
    }

    return result;
}

DBErrors CWalletDB::FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx)
{
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((std::string) "minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            std::string strType;
            ssKey >> strType;
            if (strType == "tx") {
                uint256 hash;
                ssKey >> hash;

                CWalletTx wtx;
                ssValue >> wtx;

                vTxHash.push_back(hash);
                vWtx.push_back(wtx);
            }
        }
        pcursor->close();
    } catch (const boost::thread_interrupted&) {
        throw;
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    return result;
}

DBErrors CWalletDB::ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx)
{
    // build list of wallet TXs
    std::vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(pwallet, vTxHash, vWtx);
    if (err != DB_LOAD_OK)
        return err;

    // erase each wallet TX
    for (uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DB_CORRUPT;
    }

    return DB_LOAD_OK;
}

void ThreadFlushWalletDB()
{
    // Make this thread recognisable as the wallet flushing thread
    util::ThreadRename("agenor-wallet");

    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET))
        return;

    unsigned int nLastSeen = CWalletDB::GetUpdateCounter();
    unsigned int nLastFlushed = CWalletDB::GetUpdateCounter();
    int64_t nLastWalletUpdate = GetTime();
    while (true) {
        MilliSleep(500);

        if (nLastSeen != CWalletDB::GetUpdateCounter()) {
            nLastSeen = CWalletDB::GetUpdateCounter();
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != CWalletDB::GetUpdateCounter() && GetTime() - nLastWalletUpdate >= 2) {
            TRY_LOCK(bitdb.cs_db, lockDb);
            if (lockDb) {
                // Don't do this if any databases are in use
                int nRefCount = 0;
                std::map<std::string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end()) {
                    nRefCount += (*mi).second;
                    mi++;
                }

                if (nRefCount == 0) {
                    boost::this_thread::interruption_point();
                    const std::string& strFile = pwalletMain->strWalletFile;
                    std::map<std::string, int>::iterator mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end()) {
                        LogPrint(BCLog::DB, "Flushing %s\n", strFile);
                        nLastFlushed = CWalletDB::GetUpdateCounter();
                        int64_t nStart = GetTimeMillis();

                        // Flush wallet file so it's self contained
                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);

                        bitdb.mapFileUseCount.erase(mi++);
                        LogPrint(BCLog::DB, "Flushed %s %dms\n", strFile, GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}

void NotifyBacked(const CWallet& wallet, bool fSuccess, std::string strMessage)
{
    LogPrintf("%s\n", strMessage);
    wallet.NotifyWalletBacked(fSuccess, strMessage);
}

bool BackupWallet(const CWallet& wallet, const fs::path& strDest, bool fEnableCustom)
{
    fs::path pathCustom;
    fs::path pathWithFile;
    if (!wallet.fFileBacked) {
        return false;
    } else if(fEnableCustom) {
        pathWithFile = GetArg("-backuppath", "");
        if(!pathWithFile.empty()) {
            if(!pathWithFile.has_extension()) {
                pathCustom = pathWithFile;
                pathWithFile /= wallet.GetUniqueWalletBackupName();
            } else {
                pathCustom = pathWithFile.parent_path();
            }
            try {
                fs::create_directories(pathCustom);
            } catch (const fs::filesystem_error& e) {
                NotifyBacked(wallet, false, strprintf("%s\n", e.what()));
                pathCustom = "";
            }
        }
    }

    while (true) {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) || bitdb.mapFileUseCount[wallet.strWalletFile] == 0) {
                // Flush log data to the dat file
                bitdb.CloseDb(wallet.strWalletFile);
                bitdb.CheckpointLSN(wallet.strWalletFile);
                bitdb.mapFileUseCount.erase(wallet.strWalletFile);

                // Copy wallet file
                fs::path pathDest(strDest);
                fs::path pathSrc = GetDataDir() / wallet.strWalletFile;
                if (is_directory(pathDest)) {
                    if(!exists(pathDest)) create_directory(pathDest);
                    pathDest /= wallet.strWalletFile;
                }
                bool defaultPath = AttemptBackupWallet(wallet, pathSrc.string(), pathDest.string());

                if(defaultPath && !pathCustom.empty()) {
                    int nThreshold = GetArg("-custombackupthreshold", DEFAULT_CUSTOMBACKUPTHRESHOLD);
                    if (nThreshold > 0) {

                        typedef std::multimap<std::time_t, fs::path> folder_set_t;
                        folder_set_t folderSet;
                        fs::directory_iterator end_iter;

                        pathCustom.make_preferred();
                        // Build map of backup files for current(!) wallet sorted by last write time

                        fs::path currentFile;
                        for (fs::directory_iterator dir_iter(pathCustom); dir_iter != end_iter; ++dir_iter) {
                            // Only check regular files
                            if (fs::is_regular_file(dir_iter->status())) {
                                currentFile = dir_iter->path().filename();
                                // Only add the backups for the current wallet, e.g. wallet.dat.*
                                if (dir_iter->path().stem().string() == wallet.strWalletFile) {
                                    folderSet.insert(folder_set_t::value_type(fs::last_write_time(dir_iter->path()), *dir_iter));
                                }
                            }
                        }

                        int counter = 0; //TODO: add seconds to avoid naming conflicts
                        for (auto entry : folderSet) {
                            counter++;
                            if(entry.second == pathWithFile) {
                                pathWithFile += "(1)";
                            }
                        }

                        if (counter >= nThreshold) {
                            std::time_t oldestBackup = 0;
                            for(auto entry : folderSet) {
                                if(oldestBackup == 0 || entry.first < oldestBackup) {
                                    oldestBackup = entry.first;
                                }
                            }

                            try {
                                auto entry = folderSet.find(oldestBackup);
                                if (entry != folderSet.end()) {
                                    fs::remove(entry->second);
                                    LogPrintf("Old backup deleted: %s\n", (*entry).second);
                                }
                            } catch (const fs::filesystem_error& error) {
                                std::string strMessage = strprintf("Failed to delete backup %s\n", error.what());
                                NotifyBacked(wallet, false, strMessage);
                            }
                        }
                    }
                    AttemptBackupWallet(wallet, pathSrc.string(), pathWithFile.string());
                }

                return defaultPath;
            }
        }
        MilliSleep(100);
    }
    return false;
}

bool AttemptBackupWallet(const CWallet& wallet, const fs::path& pathSrc, const fs::path& pathDest)
{
    bool retStatus;
    std::string strMessage;
    try {
        if (fs::equivalent(pathSrc, pathDest)) {
            LogPrintf("cannot backup to wallet source file %s\n", pathDest.string());
            return false;
        }
#if BOOST_VERSION >= 107400 /* BOOST_LIB_VERSION 1_74 */
        fs::copy_file(pathSrc.c_str(), pathDest, fs::copy_options::overwrite_existing);
#elif BOOST_VERSION >= 105800 /* BOOST_LIB_VERSION 1_58 */
        fs::copy_file(pathSrc.c_str(), pathDest, fs::copy_option::overwrite_if_exists);
#else
        std::ifstream src(pathSrc.c_str(),  std::ios::binary | std::ios::in);
        std::ofstream dst(pathDest.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        dst << src.rdbuf();
        dst.flush();
        src.close();
        dst.close();
#endif
        strMessage = strprintf("copied %s to %s\n", wallet.strWalletFile, pathDest.string());
        LogPrintf("%s : %s\n", __func__, strMessage);
        retStatus = true;
    } catch (const fs::filesystem_error& e) {
        retStatus = false;
        strMessage = strprintf("%s\n", e.what());
        LogPrintf("%s : %s\n", __func__, strMessage);
    }
    NotifyBacked(wallet, retStatus, strMessage);
    return retStatus;
}

//
// Try to (very carefully!) recover wallet file if there is a problem.
//
bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys)
{
    // Recovery procedure:
    // move wallet file to wallet.timestamp.bak
    // Call Salvage with fAggressive=true to
    // get as much data as possible.
    // Rewrite salvaged data to fresh wallet file.
    // Set -rescan so any missing transactions will be
    // found.
    int64_t now = GetTime();
    std::string newFilename = strprintf("wallet.%d.bak", now);

    int result = dbenv.dbenv->dbrename(NULL, filename.c_str(), NULL,
                                       newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        LogPrintf("Renamed %s to %s\n", filename, newFilename);
    else {
        LogPrintf("Failed to rename %s to %s\n", filename, newFilename);
        return false;
    }

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool allOK = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty()) {
        LogPrintf("Salvage(aggressive) found no records in %s.\n", newFilename);
        return false;
    }
    LogPrintf("Salvage(aggressive) found %u records\n", salvagedData.size());

    bool fSuccess = allOK;
    boost::scoped_ptr<Db> pdbCopy(new Db(dbenv.dbenv, 0));
    int ret = pdbCopy->open(NULL,               // Txn pointer
        filename.c_str(),   // Filename
        "main",             // Logical db name
        DB_BTREE,           // Database type
        DB_CREATE,          // Flags
        0);
    if (ret > 0) {
        LogPrintf("Cannot create database file %s\n", filename);
        return false;
    }
    CWallet dummyWallet;
    CWalletScanState wss;

    DbTxn* ptxn = dbenv.TxnBegin();
    for (CDBEnv::KeyValPair& row : salvagedData) {
        if (fOnlyKeys) {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            std::string strType, strErr;
            bool fReadOK = ReadKeyValue(&dummyWallet, ssKey, ssValue,
                wss, strType, strErr);
            if (!IsKeyType(strType))
                continue;
            if (!fReadOK) {
                LogPrintf("WARNING: CWalletDB::Recover skipping %s: %s\n", strType, strErr);
                continue;
            }
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);

    return fSuccess;
}

bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename)
{
    return CWalletDB::Recover(dbenv, filename, false);
}

bool CWalletDB::WriteDestData(const std::string& address, const std::string& key, const std::string& value)
{
    nWalletDBUpdateCounter++;
    return Write(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool CWalletDB::EraseDestData(const std::string& address, const std::string& key)
{
    nWalletDBUpdateCounter++;
    return Erase(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}

bool CWalletDB::WriteZerocoinSpendSerialEntry(const CZerocoinSpend& zerocoinSpend)
{
    return Write(std::make_pair(std::string("zcserial"), zerocoinSpend.GetSerial()), zerocoinSpend, true);
}
bool CWalletDB::EraseZerocoinSpendSerialEntry(const CBigNum& serialEntry)
{
    return Erase(std::make_pair(std::string("zcserial"), serialEntry));
}

bool CWalletDB::ReadZerocoinSpendSerialEntry(const CBigNum& bnSerial)
{
    CZerocoinSpend spend;
    return Read(std::make_pair(std::string("zcserial"), bnSerial), spend);
}

bool CWalletDB::WriteDeterministicMint(const CDeterministicMint& dMint)
{
    uint256 hash = dMint.GetPubcoinHash();
    return Write(std::make_pair(std::string("dzage"), hash), dMint, true);
}

bool CWalletDB::ReadDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint)
{
    return Read(std::make_pair(std::string("dzage"), hashPubcoin), dMint);
}

bool CWalletDB::EraseDeterministicMint(const uint256& hashPubcoin)
{
    return Erase(std::make_pair(std::string("dzage"), hashPubcoin));
}

bool CWalletDB::WriteZerocoinMint(const CZerocoinMint& zerocoinMint)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << zerocoinMint.GetValue();
    uint256 hash = Hash(ss.begin(), ss.end());

    Erase(std::make_pair(std::string("zerocoin"), hash));
    return Write(std::make_pair(std::string("zerocoin"), hash), zerocoinMint, true);
}

bool CWalletDB::ReadZerocoinMint(const CBigNum &bnPubCoinValue, CZerocoinMint& zerocoinMint)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnPubCoinValue;
    uint256 hash = Hash(ss.begin(), ss.end());

    return ReadZerocoinMint(hash, zerocoinMint);
}

bool CWalletDB::ReadZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint)
{
    return Read(std::make_pair(std::string("zerocoin"), hashPubcoin), mint);
}

bool CWalletDB::EraseZerocoinMint(const CZerocoinMint& zerocoinMint)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << zerocoinMint.GetValue();
    uint256 hash = Hash(ss.begin(), ss.end());

    return Erase(std::make_pair(std::string("zerocoin"), hash));
}

bool CWalletDB::ArchiveMintOrphan(const CZerocoinMint& zerocoinMint)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << zerocoinMint.GetValue();
    uint256 hash = Hash(ss.begin(), ss.end());;

    if (!Write(std::make_pair(std::string("zco"), hash), zerocoinMint)) {
        LogPrintf("%s : failed to database orphaned zerocoin mint\n", __func__);
        return false;
    }

    if (!Erase(std::make_pair(std::string("zerocoin"), hash))) {
        LogPrintf("%s : failed to erase orphaned zerocoin mint\n", __func__);
        return false;
    }

    return true;
}

bool CWalletDB::ArchiveDeterministicOrphan(const CDeterministicMint& dMint)
{
    if (!Write(std::make_pair(std::string("dzco"), dMint.GetPubcoinHash()), dMint))
        return error("%s: write failed", __func__);

    if (!Erase(std::make_pair(std::string("dzage"), dMint.GetPubcoinHash())))
        return error("%s: failed to erase", __func__);

    return true;
}

bool CWalletDB::UnarchiveDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint)
{
    if (!Read(std::make_pair(std::string("dzco"), hashPubcoin), dMint))
        return error("%s: failed to retrieve deterministic mint from archive", __func__);

    if (!WriteDeterministicMint(dMint))
        return error("%s: failed to write deterministic mint", __func__);

    if (!Erase(std::make_pair(std::string("dzco"), dMint.GetPubcoinHash())))
        return error("%s : failed to erase archived deterministic mint", __func__);

    return true;
}

bool CWalletDB::UnarchiveZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint)
{
    if (!Read(std::make_pair(std::string("zco"), hashPubcoin), mint))
        return error("%s: failed to retrieve zerocoinmint from archive", __func__);

    if (!WriteZerocoinMint(mint))
        return error("%s: failed to write zerocoinmint", __func__);

    uint256 hash = GetPubCoinHash(mint.GetValue());
    if (!Erase(std::make_pair(std::string("zco"), hash)))
        return error("%s : failed to erase archived zerocoin mint", __func__);

    return true;
}

bool CWalletDB::WriteCurrentSeedHash(const uint256& hashSeed)
{
    return Write(std::string("seedhash"), hashSeed);
}

bool CWalletDB::ReadCurrentSeedHash(uint256& hashSeed)
{
    return Read(std::string("seedhash"), hashSeed);
}

bool CWalletDB::WriteZAgenoreed(const uint256& hashSeed, const std::vector<unsigned char>& seed)
{
    if (!WriteCurrentSeedHash(hashSeed))
        return error("%s: failed to write current seed hash", __func__);

    return Write(std::make_pair(std::string("dzs"), hashSeed), seed);
}

bool CWalletDB::EraseZAgenoreed()
{
    uint256 hash;
    if(!ReadCurrentSeedHash(hash)){
        return error("Failed to read a current seed hash");
    }
    if(!WriteZAgenoreed(hash, ToByteVector(base_uint<256>(0) << 256))) {
        return error("Failed to write empty seed to wallet");
    }
    if(!WriteCurrentSeedHash(UINT256_ZERO)) {
        return error("Failed to write empty seedHash");
    }

    return true;
}

bool CWalletDB::EraseZAgenoreed_deprecated()
{
    return Erase(std::string("dzs"));
}

bool CWalletDB::ReadZAgenoreed(const uint256& hashSeed, std::vector<unsigned char>& seed)
{
    return Read(std::make_pair(std::string("dzs"), hashSeed), seed);
}

bool CWalletDB::ReadZAgenoreed_deprecated(uint256& seed)
{
    return Read(std::string("dzs"), seed);
}

bool CWalletDB::WriteZAGECount(const uint32_t& nCount)
{
    return Write(std::string("dzc"), nCount);
}

bool CWalletDB::ReadZAGECount(uint32_t& nCount)
{
    return Read(std::string("dzc"), nCount);
}

bool CWalletDB::WriteMintPoolPair(const uint256& hashMasterSeed, const uint256& hashPubcoin, const uint32_t& nCount)
{
    return Write(std::make_pair(std::string("mintpool"), hashPubcoin), std::make_pair(hashMasterSeed, nCount));
}


//! map with hashMasterSeed as the key, paired with vector of hashPubcoins and their count
std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > CWalletDB::MapMintPool()
{
    std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > mapPool;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << std::make_pair(std::string("mintpool"), UINT256_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "mintpool")
            break;

        uint256 hashPubcoin;
        ssKey >> hashPubcoin;

        uint256 hashMasterSeed;
        ssValue >> hashMasterSeed;

        uint32_t nCount;
        ssValue >> nCount;

        std::pair<uint256, uint32_t> pMint;
        pMint.first = hashPubcoin;
        pMint.second = nCount;
        if (mapPool.count(hashMasterSeed)) {
            mapPool.at(hashMasterSeed).emplace_back(pMint);
        } else {
            std::vector<std::pair<uint256, uint32_t> > vPairs;
            vPairs.emplace_back(pMint);
            mapPool.insert(std::make_pair(hashMasterSeed, vPairs));
        }
    }

    pcursor->close();

    return mapPool;
}

std::list<CDeterministicMint> CWalletDB::ListDeterministicMints()
{
    std::list<CDeterministicMint> listMints;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(std::string("dzage"), UINT256_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "dzage")
            break;

        uint256 hashPubcoin;
        ssKey >> hashPubcoin;

        CDeterministicMint mint;
        ssValue >> mint;

        listMints.emplace_back(mint);
    }

    pcursor->close();
    return listMints;
}

std::list<CZerocoinMint> CWalletDB::ListMintedCoins()
{
    std::list<CZerocoinMint> listPubCoin;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    std::vector<CZerocoinMint> vOverWrite;
    std::vector<CZerocoinMint> vArchive;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(std::string("zerocoin"), UINT256_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "zerocoin")
            break;

        uint256 hashPubcoin;
        ssKey >> hashPubcoin;

        CZerocoinMint mint;
        ssValue >> mint;

        listPubCoin.emplace_back(mint);
    }

    pcursor->close();
    return listPubCoin;
}

std::list<CZerocoinSpend> CWalletDB::ListSpentCoins()
{
    std::list<CZerocoinSpend> listCoinSpend;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(std::string("zcserial"), BN_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "zcserial")
            break;

        CBigNum value;
        ssKey >> value;

        CZerocoinSpend zerocoinSpendItem;
        ssValue >> zerocoinSpendItem;

        listCoinSpend.push_back(zerocoinSpendItem);
    }

    pcursor->close();
    return listCoinSpend;
}

// Just get the Serial Numbers
std::list<CBigNum> CWalletDB::ListSpentCoinsSerial()
{
    std::list<CBigNum> listPubCoin;
    std::list<CZerocoinSpend> listCoins = ListSpentCoins();

    for ( auto& coin : listCoins) {
        listPubCoin.push_back(coin.GetSerial());
    }
    return listPubCoin;
}

std::list<CZerocoinMint> CWalletDB::ListArchivedZerocoins()
{
    std::list<CZerocoinMint> listMints;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(std::string("zco"), BN_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "zco")
            break;

        uint256 value;
        ssKey >> value;

        CZerocoinMint mint;
        ssValue >> mint;

        listMints.push_back(mint);
    }

    pcursor->close();
    return listMints;
}

std::list<CDeterministicMint> CWalletDB::ListArchivedDeterministicMints()
{
    std::list<CDeterministicMint> listMints;
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__)+" : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    for (;;)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(std::string("dzco"), BN_ZERO);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__)+" : error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "dzco")
            break;

        uint256 value;
        ssKey >> value;

        CDeterministicMint dMint;
        ssValue >> dMint;

        listMints.emplace_back(dMint);
    }

    pcursor->close();
    return listMints;
}

void CWalletDB::IncrementUpdateCounter()
{
    nWalletDBUpdateCounter++;
}

unsigned int CWalletDB::GetUpdateCounter()
{
    return nWalletDBUpdateCounter;
}
