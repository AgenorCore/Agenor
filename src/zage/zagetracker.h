// Copyright (c) 2018-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Agenor_ZAGETRACKER_H
#define Agenor_ZAGETRACKER_H

#include "zerocoin.h"
#include "sync.h"
#include <list>

class CDeterministicMint;
class CzAGEWallet;
class CWallet;

class CzAGETracker
{
private:
    bool fInitialized;
    /* Parent wallet */
    CWallet* wallet{nullptr};
    std::map<uint256, CMintMeta> mapSerialHashes;
    std::map<uint256, uint256> mapPendingSpends; //serialhash, txid of spend
    bool UpdateStatusInternal(const std::set<uint256>& setMempool, CMintMeta& mint);
public:
    CzAGETracker(CWallet* parent);
    ~CzAGETracker();
    void Add(const CDeterministicMint& dMint, bool isNew = false, bool isArchived = false, CzAGEWallet* zAGEWallet = NULL);
    void Add(const CZerocoinMint& mint, bool isNew = false, bool isArchived = false);
    bool Archive(CMintMeta& meta);
    bool HasPubcoin(const CBigNum& bnValue) const;
    bool HasPubcoinHash(const uint256& hashPubcoin) const;
    bool HasSerial(const CBigNum& bnSerial) const;
    bool HasSerialHash(const uint256& hashSerial) const;
    bool HasMintTx(const uint256& txid);
    bool IsEmpty() const { return mapSerialHashes.empty(); }
    void Init();
    CMintMeta Get(const uint256& hashSerial);
    CMintMeta GetMetaFromPubcoin(const uint256& hashPubcoin);
    bool GetMetaFromStakeHash(const uint256& hashStake, CMintMeta& meta) const;
    CAmount GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const;
    std::vector<uint256> GetSerialHashes();
    std::vector<CMintMeta> GetMints(bool fConfirmedOnly) const;
    CAmount GetUnconfirmedBalance() const;
    std::set<CMintMeta> ListMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus, bool fWrongSeed = false, bool fExcludeV1 = false);
    void RemovePending(const uint256& txid);
    void SetPubcoinUsed(const uint256& hashPubcoin, const uint256& txid);
    void SetPubcoinNotUsed(const uint256& hashPubcoin);
    bool UnArchive(const uint256& hashPubcoin, bool isDeterministic);
    bool UpdateZerocoinMint(const CZerocoinMint& mint);
    bool UpdateState(const CMintMeta& meta);
    void Clear();
};

#endif //PIVX_ZPIVTRACKER_H
