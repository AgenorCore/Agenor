// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLETDB_H
#define BITCOIN_WALLETDB_H

#include "amount.h"
#include "wallet/db.h"
#include "wallet/hdchain.h"
#include "key.h"
#include "keystore.h"
#include "script/keyorigin.h"
#include "zage/zerocoin.h"
#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Denominations.h"
#include "zage/zagetracker.h"

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

static const bool DEFAULT_FLUSHWALLET = true;

class CAccount;
class CAccountingEntry;
struct CBlockLocator;
class CKeyPool;
class CMasterKey;
class CScript;
class CWallet;
class CWalletTx;
class CDeterministicMint;
class CZerocoinMint;
class CZerocoinSpend;
class uint160;
class uint256;

/** Error statuses for the wallet database */
enum DBErrors {
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata
{
public:
    // Metadata versions
    static const int VERSION_BASIC = 1;
    static const int VERSION_WITH_KEY_ORIGIN = 12;
    // Active version
    static const int CURRENT_VERSION = VERSION_WITH_KEY_ORIGIN;

    int nVersion;
    int64_t nCreateTime; // 0 means unknown
    CKeyID hd_seed_id; //id of the HD seed used to derive this key
    KeyOriginInfo key_origin; // Key origin info with path and fingerprint

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        SetNull();
        nCreateTime = nCreateTime_;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(nCreateTime);
        if (HasKeyOrigin()) {
            READWRITE(hd_seed_id);
            READWRITE(key_origin);
        }
    }

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
        hd_seed_id.SetNull();
        key_origin.clear();
    }

    bool HasKeyOrigin() const
    {
        return this->nVersion >= VERSION_WITH_KEY_ORIGIN;
    }
};

/** Access to the wallet database */
class CWalletDB : public CDB
{
public:
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+", bool fFlushOnClose = true) : CDB(strFilename, pszMode, fFlushOnClose)
    {
    }

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata& keyMeta);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript& script);
    bool EraseWatchOnly(const CScript& script);

    bool WriteMultiSig(const CScript& script);
    bool EraseMultiSig(const CScript& script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool WriteStakeSplitThreshold(const CAmount& nStakeSplitThreshold);
    bool WriteUseCustomFee(bool fUse);
    bool WriteCustomFeeValue(const CAmount& nCustomFee);
    bool WriteMultiSend(std::vector<std::pair<std::string, int> > vMultiSend);
    bool EraseMultiSend(std::vector<std::pair<std::string, int> > vMultiSend);
    bool WriteMSettings(bool fMultiSendStake, bool fMultiSendMasternode, int nLastMultiSendHeight);
    bool WriteMSDisabledAddresses(std::vector<std::string> vDisabledAddresses);
    bool EraseMSDisabledAddresses(std::vector<std::string> vDisabledAddresses);
    bool WriteAutoCombineSettings(bool fEnable, CAmount nCombineThreshold);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(int64_t nPool);

    bool WriteMinVersion(int nVersion);

    /// This writes directly to the database, and will not update the CWallet's cached accounting entries!
    /// Use wallet.AddAccountingEntry instead, to write *and* update its caches.
    bool WriteAccountingEntry_Backend(const CAccountingEntry& acentry);

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);
    bool EraseAccount(const std::string& strAccount);

    //! write the hdchain model (external/internal chain child index counter)
    bool WriteHDChain(const CHDChain& chain);

    /// Write extended spending key to wallet database, where the key is the incoming viewing key
    bool WriteSaplingZKey(const libzcash::SaplingIncomingViewingKey &ivk,
                          const libzcash::SaplingExtendedSpendingKey &key,
                          const CKeyMetadata  &keyMeta);

    bool WriteSaplingPaymentAddress(const libzcash::SaplingPaymentAddress &addr,
                                    const libzcash::SaplingIncomingViewingKey &ivk);

    bool WriteCryptedSaplingZKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                 const std::vector<unsigned char>& vchCryptedSecret,
                                 const CKeyMetadata &keyMeta);

    /// Write destination data key,value tuple to database
    bool WriteDestData(const std::string& address, const std::string& key, const std::string& value);
    /// Erase destination data tuple from wallet database
    bool EraseDestData(const std::string& address, const std::string& key);

    CAmount GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet* pwallet);
    DBErrors LoadWallet(CWallet* pwallet);
    DBErrors FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx);
    static bool Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, std::string filename);

    bool WriteDeterministicMint(const CDeterministicMint& dMint);
    bool ReadDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint);
    bool EraseDeterministicMint(const uint256& hashPubcoin);
    bool WriteZerocoinMint(const CZerocoinMint& zerocoinMint);
    bool EraseZerocoinMint(const CZerocoinMint& zerocoinMint);
    bool ReadZerocoinMint(const CBigNum &bnPubcoinValue, CZerocoinMint& zerocoinMint);
    bool ReadZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint);
    bool ArchiveMintOrphan(const CZerocoinMint& zerocoinMint);
    bool ArchiveDeterministicOrphan(const CDeterministicMint& dMint);
    bool UnarchiveZerocoinMint(const uint256& hashPubcoin, CZerocoinMint& mint);
    bool UnarchiveDeterministicMint(const uint256& hashPubcoin, CDeterministicMint& dMint);
    std::list<CZerocoinMint> ListMintedCoins();
    std::list<CDeterministicMint> ListDeterministicMints();
    std::list<CZerocoinSpend> ListSpentCoins();
    std::list<CBigNum> ListSpentCoinsSerial();
    std::list<CZerocoinMint> ListArchivedZerocoins();
    std::list<CDeterministicMint> ListArchivedDeterministicMints();
    bool WriteZerocoinSpendSerialEntry(const CZerocoinSpend& zerocoinSpend);
    bool EraseZerocoinSpendSerialEntry(const CBigNum& serialEntry);
    bool ReadZerocoinSpendSerialEntry(const CBigNum& bnSerial);
    bool WriteCurrentSeedHash(const uint256& hashSeed);
    bool ReadCurrentSeedHash(uint256& hashSeed);
    bool WriteZAgenoreed(const uint256& hashSeed, const std::vector<unsigned char>& seed);
    bool ReadZAgenoreed(const uint256& hashSeed, std::vector<unsigned char>& seed);
    bool ReadZAgenoreed_deprecated(uint256& seed);
    bool EraseZAgenoreed();
    bool EraseZAgenoreed_deprecated();

    bool WriteZAGECount(const uint32_t& nCount);
    bool ReadZAGECount(uint32_t& nCount);
    std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > MapMintPool();
    bool WriteMintPoolPair(const uint256& hashMasterSeed, const uint256& hashPubcoin, const uint32_t& nCount);

    static void IncrementUpdateCounter();
    static unsigned int GetUpdateCounter();
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);

    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
};

void NotifyBacked(const CWallet& wallet, bool fSuccess, std::string strMessage);
bool BackupWallet(const CWallet& wallet, const fs::path& strDest, bool fEnableCustom = true);
bool AttemptBackupWallet(const CWallet& wallet, const fs::path& pathSrc, const fs::path& pathDest);

void ThreadFlushWalletDB();

#endif // BITCOIN_WALLETDB_H
