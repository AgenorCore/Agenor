// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "primitives/block.h"

#include <stdint.h>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class COutput;
class CReserveKey;
class CScript;
class CWallet;

static const bool DEFAULT_PRINTPRIORITY = false;

struct CBlockTemplate;

/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake, std::vector<COutput>* availableCoins = nullptr);
/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/** Check mined block */
void UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev);

#ifdef ENABLE_WALLET
    /** Run the miner threads */
    void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads);
    /** Generate a new block, without valid proof-of-work */
    CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet);

    void BitcoinMiner(CWallet* pwallet, bool fProofOfStake);
    void ThreadStakeMinter();
#endif // ENABLE_WALLET

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

struct CBlockTemplate {
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

#endif // BITCOIN_MINER_H
